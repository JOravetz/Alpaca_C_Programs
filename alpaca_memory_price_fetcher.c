#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <jansson.h>
#include <curl/curl.h>

#define URL_SIZE 500
#define ALPACA_API_KEY getenv("APCA_API_KEY_ID")
#define ALPACA_SECRET_KEY getenv("APCA_API_SECRET_KEY")
#define ALPACA_ENDPOINT "https://data.alpaca.markets/v2"

// Macro to get either the real or integer value of a json_t object
#define GET_JSON_REAL_OR_INTEGER(json) \
  (json_is_real(json) ? json_real_value(json) : json_integer_value(json))

typedef struct {
  char* data;
  size_t size;
} MemoryStruct;

// This function is called by libcurl when it receives data.
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  MemoryStruct *mem = (MemoryStruct *)userp;

  char* ptr = realloc(mem->data, mem->size + realsize + 1);
  if(ptr == NULL) {
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->data = ptr;
  memcpy(&(mem->data[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->data[mem->size] = 0;

  return realsize;
}

// Function to fetch JSON data from the Alpaca API
char *fetch_json_data(char *symbol, char *timeframe, char *start_date, char *end_date, int limit, char *sip, char *next_page_token) {
    CURL *curl;
    CURLcode res;
    char url[URL_SIZE];
    char* url_format = "%s&page_token=%s";
    char* base_url = "https://data.alpaca.markets/v2/stocks/%s/bars?timeframe=%s&start=%s&end=%s&limit=%d&feed=%s";
    MemoryStruct chunk;

    chunk.data = malloc(1);
    chunk.size = 0;

    /*
    // Get the API key ID and secret key from environment variables
    const char *api_key_id = getenv("APCA_API_KEY_ID");
    const char *api_secret_key = getenv("APCA_API_SECRET_KEY");
    */

    // Check that the keys were found
    if (!ALPACA_API_KEY || !ALPACA_SECRET_KEY) {
      fprintf(stderr, "API key ID and/or secret key not found in environment variables\n");
      return 1;
    }

    // Construct the URL for the API request
    // Check if next_page_token is NULL
    if (next_page_token == NULL) {
      snprintf(url, URL_SIZE, base_url, symbol, timeframe, start_date, end_date, limit, sip);
    }
    else {
      // Append next_page_token to the URL
      char* url_with_token = malloc(strlen(base_url) + strlen(next_page_token) + strlen(url_format) + 1);
      sprintf(url_with_token, url_format, base_url, next_page_token);
      snprintf(url, URL_SIZE, url_with_token, symbol, timeframe, start_date, end_date, limit, sip);
      free(url_with_token);
    }

    // Initialize a curl handle
    curl = curl_easy_init();
    if(curl) {
        // Set the headers for the API request
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char auth_header[128];
        snprintf(auth_header, 128, "APCA-API-KEY-ID: %s", ALPACA_API_KEY);
        headers = curl_slist_append(headers, auth_header);
        char secret_header[128];
        snprintf(secret_header, 128, "APCA-API-SECRET-KEY: %s", ALPACA_SECRET_KEY);
        headers = curl_slist_append(headers, secret_header);

        // Set options for the curl handle
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);

	// Check for errors in the curl request
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.data);
	    exit(0);
        }

	// Cleanup curl handle and headers
        curl_easy_cleanup(curl);
    	curl_slist_free_all(headers);
    }
    return chunk.data; // return the received data
}

// Define global variable
size_t total_bars_count = 0;

// Convert from UTC timestring to local time format
char* utc_to_local(char* utc_time_str) {
    // Parse UTC time string
    struct tm utc_tm = {0};
    strptime(utc_time_str, "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
    time_t utc_time = timegm(&utc_tm);

    // Convert to local time using pre-loaded timezone information
    struct tm local_tm = {0};
    localtime_r(&utc_time, &local_tm);

    // Format local time string
    char* local_time_str = (char*) malloc(20);
    strftime(local_time_str, 25, "%Y-%m-%d %H:%M:%S %Z", &local_tm);

    return local_time_str;
}

char* parse_json_data(char* json_data) {
  char* next_page_token = NULL;
  const char* time_format = "%Y-%m-%d %H:%M:%S CST"; // Format string for the timestamp
  // Get the JSON objects for the bar fields
  json_t *alpaca_time, *open, *high, *low, *close, *volume, *trade_count, *weighted_volume;

  // Parse the received JSON using jansson
  json_error_t error;
  json_t *root = json_loads(json_data, 0, &error);
  free(json_data);

  if (!root) {
    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    return NULL;
  }

  // Extract and print the desired data
  json_t *bars = json_object_get(root, "bars");
  if (!json_is_array(bars)) {
    fprintf(stderr, "Error: 'bars' is not an array.\n");
    fprintf(stderr, "\n");
    json_decref(root);
    return NULL;
  }

  // Extract the next page token (if any) and print it out
  json_t* next_page_token_json = json_object_get(root, "next_page_token");
  if (json_is_string(next_page_token_json)) {
    next_page_token = strdup(json_string_value(next_page_token_json));
    // printf("Next page token: %s\n", next_page_token);
  } else {
    next_page_token = NULL;
    // printf("Next page token not found\n");
  }

  size_t bars_count = json_array_size(bars);

  for (size_t i = 0; i < bars_count; i++) {
    json_t *bar = json_array_get(bars, i);

    // Parse the UTC time
    alpaca_time = json_object_get(bar, "t");
    const char *utc_time_str = json_string_value(alpaca_time);
    // strptime(utc_time_str, "%Y-%m-%dT%H:%M:%SZ", &utc_tm);

    char* local_time_str = utc_to_local(utc_time_str);

    // Get the values for the bar fields
    open = json_object_get(bar, "o");
    high = json_object_get(bar, "h");
    low = json_object_get(bar, "l");
    close = json_object_get(bar, "c");
    volume = json_object_get(bar, "v");
    trade_count = json_object_get(bar, "n");
    weighted_volume = json_object_get(bar, "vw");
    double open_value = GET_JSON_REAL_OR_INTEGER(open);
    double high_value = GET_JSON_REAL_OR_INTEGER(high);
    double low_value = GET_JSON_REAL_OR_INTEGER(low);
    double close_value = GET_JSON_REAL_OR_INTEGER(close);

    // Print the bar information
    printf("Bar %6zu: Time=%s, Open=%.3f, High=%.3f, Low=%.3f, Close=%.3f, Volume=%8lld, Trade_Count=%5lld, Weighted_Volume=%.3f\n",
      total_bars_count+i+1, local_time_str, open_value, high_value, low_value, close_value, json_integer_value(volume), json_integer_value(trade_count),
      json_real_value(weighted_volume));
  }
  total_bars_count += bars_count;
  json_decref(root);
  return next_page_token;
}

double get_latest_trade(char* symbol, char* sip) {
  CURL *curl;
  CURLcode res;
  MemoryStruct chunk;

  chunk.data = malloc(1);
  chunk.size = 0;

  curl_global_init(CURL_GLOBAL_ALL);

  curl = curl_easy_init();
  if(curl) {
    // Convert symbol to uppercase
    int len = strlen(symbol);
    for (int i = 0; i < len; i++) {
      symbol[i] = toupper(symbol[i]);
    }

    char url[256];
    snprintf(url, 256, "%s/stocks/%s/trades/latest?feed=%s", ALPACA_ENDPOINT, symbol, sip);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[1024];
    snprintf(auth_header, 1024, "APCA-API-KEY-ID: %s", ALPACA_API_KEY);
    headers = curl_slist_append(headers, auth_header);
    char auth_secret_header[1024];
    snprintf(auth_secret_header, 1024, "APCA-API-SECRET-KEY: %s", ALPACA_SECRET_KEY);
    headers = curl_slist_append(headers, auth_secret_header);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      curl_easy_cleanup(curl);
      curl_global_cleanup();
      free(chunk.data);
      return -1.0;
    }

    double price = -1.0;

    json_error_t error;
    json_t *root = json_loads(chunk.data, 0, &error);
    if (root != NULL) {
      json_t *trade = json_object_get(root, "trade");
      if (json_is_object(trade)) {
        json_t *price_item = json_object_get(trade, "p");
        if (json_is_number(price_item)) {
          price = json_number_value(price_item);
        }
      }
      json_decref(root);
    }

    json_decref(root);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    free(chunk.data);

    return price;
  }

  curl_global_cleanup();
  free(chunk.data);

  return -1.0;
}

// Main C program that fetches JSON data from Alpaca API based on user input
int main(int argc, char *argv[]) {
    const int limit = 10000;
    char *symbol = NULL;
    char *start_date = NULL;
    char *end_date = NULL;
    const char *timeframe = "1Min";
    const char *sip = "sip";
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char default_end_date[11];
    char* next_page_token = NULL;
    char* json_data = NULL; // to store the received JSON data
    bool first_fetch = true; // to check if this is the first fetch

    // Pre-load timezone database into memory
    tzset();

    // Set the default end date to the current date
    sprintf(default_end_date, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
	// If the argument is "-symbol" and a symbol is provided, set the symbol and convert to uppercase
	if (strcmp(argv[i], "-symbol") == 0 && i < argc - 1) {
	    symbol = argv[i+1];
	    if (islower(*symbol)) {
		*symbol = toupper(*symbol);
	    }
	}
	// If the argument is "-start" and a start date is provided, set the start date
	else if (strcmp(argv[i], "-start") == 0 && i < argc - 1) {
	    start_date = argv[i+1];
	}
	// If the argument is "-end" and an end date is provided, set the end date
	else if (strcmp(argv[i], "-end") == 0 && i < argc - 1) {
	    end_date = argv[i+1];
	}
	// If the argument is "-timeframe" and a timeframe is provided, set the timeframe
	else if (strcmp(argv[i], "-timeframe") == 0 && i < argc - 1) {
	    timeframe = argv[i+1];
	}
	// If the argument is "-sip" and a feed is provided, set the feed
	else if (strcmp(argv[i], "-sip") == 0 && i < argc - 1) {
	    sip = argv[i+1];
	}
    }

    // If no end date is provided, set the default end date
    if (start_date == NULL) {
	start_date = default_end_date;
    }

    // If no end date is provided, set the default end date
    if (end_date == NULL) {
	end_date = default_end_date;
    }

    // If any required command line arguments are missing, print an error message and return
    if (!symbol) {
	fprintf(stderr, "Missing command-line arguments.\nUsage: %s -symbol <symbol> -start <start_date> -end <end_date>\n", argv[0]);
	return 1;
    }

    while (true) {
       // Fetch JSON data from Alpaca API
       json_data = fetch_json_data(symbol, timeframe, start_date, end_date, limit, sip, next_page_token);

       // Parse the JSON data and store the result
       next_page_token = parse_json_data(json_data);
       if (next_page_token != NULL) {
         if (first_fetch) {
           // printf("%s\n", next_page_token);
           first_fetch = false;
         }
       } else {
         // If there is no next page token, break out of the loop
         break;
       }
    }

    double price = get_latest_trade(symbol,sip);
    if (price >= 0) {
      printf("Latest trade price for %s: %.3f\n", symbol, price);
    } else {
      printf("Failed to retrieve latest trade price for %s.\n", symbol);
    }

    // Clean up the curl global environment
    curl_global_cleanup();

    // Return 0 to indicate success
    return 0;
}
