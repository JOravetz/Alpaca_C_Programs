/*
This C-program retrieves and displays the latest trade price for a given stock symbol using the Alpaca API.
It makes an API request using libcurl, parses the JSON response with the Jansson library, and outputs the result.
To compile, link against libcurl and libjansson, and set the APCA_API_KEY_ID and APCA_API_SECRET_KEY environment
variables to your Alpaca API credentials.
Example usage:
  ./alpaca-current-price-fetcher_jansson AAPL
Output: "Latest trade price for AAPL: 150.230"
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#define ALPACA_API_KEY getenv("APCA_API_KEY_ID")
#define ALPACA_SECRET_KEY getenv("APCA_API_SECRET_KEY")
#define INITIAL_CHUNK_SIZE 1
const char *ALPACA_ENDPOINT = "https://data.alpaca.markets/v2";
typedef struct {
    char *data;
    size_t size;
} MemoryStruct;
MemoryStruct init_memory_struct() {
    MemoryStruct chunk;
    chunk.data = malloc(INITIAL_CHUNK_SIZE);
    if (!chunk.data) {
        perror("Failed to allocate memory for initial chunk");
        exit(EXIT_FAILURE);
    }
    chunk.size = 0;
    return chunk;
}
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        perror("not enough memory (realloc returned NULL)");
        return 0;
    }
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}
int get_latest_trade(const char *symbol, const char *sip, double *price) {
    int result;
    if (!ALPACA_API_KEY || !ALPACA_SECRET_KEY) {
        fprintf(stderr, "Error: APCA_API_KEY_ID and APCA_API_SECRET_KEY environment variables must be set.\n");
        return -1.0;
    }
    CURL *curl;
    CURLcode res;
    MemoryStruct chunk = init_memory_struct();
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        free(chunk.data);
        curl_global_cleanup();
        return -1.0;
    }
    char url[256];
    snprintf(url, sizeof(url), "%s/stocks/%s/trades/latest?feed=%s", ALPACA_ENDPOINT, symbol, sip);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_headers[2048];
    snprintf(auth_headers, sizeof(auth_headers), "APCA-API-KEY-ID: %s\nAPCA-API-SECRET-KEY: %s", ALPACA_API_KEY, ALPACA_SECRET_KEY);
    headers = curl_slist_append(headers, auth_headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        json_error_t error;
        json_t *root = json_loads(chunk.data, 0, &error);
        if (root) {
            json_t *trade = json_object_get(root, "trade");
            if (json_is_object(trade)) {
                json_t *price_item = json_object_get(trade, "p");
                if (json_is_number(price_item)) {
                    *price = json_number_value(price_item);
		    result = 0;
                }
            }
            json_decref(root);
        }
    } else {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	result = -1;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    free(chunk.data);
    return result;
}
void convert_to_upper(char *str) {
    for (; *str; ++str) {
        *str = toupper(*str);
    }
}
int main(int argc, char **argv) {
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Usage: %s -s SYMBOL [-sip SIP]\n", argv[0]);
        exit(1);
    }

    char *symbol = NULL;
    char *sip = "sip";

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-s") == 0) {
            symbol = argv[i + 1];
        } else if (strcmp(argv[i], "-sip") == 0) {
            sip = argv[i + 1];
        }
    }

    if (!symbol) {
        fprintf(stderr, "Error: -s SYMBOL argument is required.\n");
        exit(1);
    }

    if (strcmp(sip, "sip") != 0 && strcmp(sip, "iex") != 0) {
        fprintf(stderr, "Error: -sip value must be either 'sip' or 'iex'.\n");
        exit(1);
    }

    convert_to_upper(symbol);
    double price;
    int result = get_latest_trade(symbol, sip, &price); // Modify get_latest_trade to accept and use the sip parameter
    if (result == 0) {
        printf("Latest trade price for %s: %.3f\n", symbol, price);
    } else {
        printf("Failed to retrieve latest trade price for %s.\n", symbol);
    }
    return 0;
}
