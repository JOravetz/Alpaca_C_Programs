#include "alpaca_lib_jansson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <libwebsockets.h>
#include <unistd.h>
#include <jansson.h>
#include <ctype.h>
#include <time.h>

int interrupted = 0;

void parse_bar_data(const char *json_data) {
    json_error_t error;
    json_t *root;

    // Load the JSON data
    root = json_loads(json_data, 0, &error);
    if (!root) {
        fprintf(stderr, "Error parsing JSON data: %s\n", error.text);
        return;
    }

    // Ensure the JSON data is an object
    if (!json_is_object(root)) {
        fprintf(stderr, "Error: JSON data is not an object.\n");
        json_decref(root);
        return;
    }

    printf("JSON data before parsing: %s\n", json_data);

    // Extract the bar data from the JSON object
    const char *msg_type, *symbol, *timestamp;
    double open, high, low, close, vw;
    int volume, trades;

    msg_type = json_string_value(json_object_get(root, "T"));
    symbol = json_string_value(json_object_get(root, "S"));
    open = json_number_value(json_object_get(root, "o"));
    high = json_number_value(json_object_get(root, "h"));
    low = json_number_value(json_object_get(root, "l"));
    close = json_number_value(json_object_get(root, "c"));
    volume = json_integer_value(json_object_get(root, "v"));
    timestamp = json_string_value(json_object_get(root, "t"));
    trades = json_integer_value(json_object_get(root, "n"));
    vw = json_number_value(json_object_get(root, "vw"));

    // Print the bar data
    printf("Bar Data:\n");
    printf("  Message Type: %s\n", msg_type);
    printf("  Symbol: %s\n", symbol);
    printf("  Open: %.3f\n", open);
    printf("  High: %.3f\n", high);
    printf("  Low: %.3f\n", low);
    printf("  Close: %.3f\n", close);
    printf("  Volume: %d\n", volume);
    printf("  Timestamp: %s\n", timestamp);
    printf("  Number of Trades: %d\n", trades);
    printf("  VWAP: %.5f\n", vw);

    // Print the local time using the provided function
    print_local_time(timestamp);
    printf("\n");

    // Free the JSON object
    json_decref(root);
}

void print_local_time(const char *timestamp) {
    struct tm tm;
    time_t t;
    int milliseconds;

    memset(&tm, 0, sizeof(struct tm));
    sscanf(timestamp, "%d-%d-%dT%d:%d:%d.%dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &milliseconds);

    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    t = mktime(&tm);
    t -= timezone;

    struct tm *local_tm = localtime(&t);

    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_tm);
    printf("  Local Time: %s.%03d %s\n", buffer, milliseconds, local_tm->tm_zone);
}

void parse_quote_data(const char *json_data) {
    json_error_t error;
    json_t *root, *quote_object;

    // Load the JSON data
    root = json_loads(json_data, 0, &error);
    if (!root) {
        fprintf(stderr, "Error parsing JSON data: %s\n", error.text);
        return;
    }

    // Ensure the JSON data is an object
    if (!json_is_object(root)) {
        fprintf(stderr, "Error: JSON data is not an object.\n");
        json_decref(root);
        return;
    }

    printf("JSON data before parsing: %s\n", json_data);

    // Extract the quote data from the JSON object
    const char *msg_type, *symbol, *bid_exchange, *ask_exchange, *timestamp;
    double bid_price, ask_price;
    int bid_size, ask_size;

    msg_type = json_string_value(json_object_get(root, "T"));
    symbol = json_string_value(json_object_get(root, "S"));
    bid_exchange = json_string_value(json_object_get(root, "bx"));
    bid_price = json_number_value(json_object_get(root, "bp"));
    bid_size = json_integer_value(json_object_get(root, "bs"));
    ask_exchange = json_string_value(json_object_get(root, "ax"));
    ask_price = json_number_value(json_object_get(root, "ap"));
    ask_size = json_integer_value(json_object_get(root, "as"));
    timestamp = json_string_value(json_object_get(root, "t"));

    // Print the quote data
    printf("Quote Data:\n");
    printf("  Message Type: %s\n", msg_type);
    printf("  Symbol: %s\n", symbol);
    printf("  Bid Exchange: %s\n", bid_exchange);
    printf("  Bid Price: %.3f\n", bid_price);
    printf("  Bid Size: %d\n", bid_size);
    printf("  Ask Exchange: %s\n", ask_exchange);
    printf("  Ask Price: %.3f\n", ask_price);
    printf("  Ask Size: %d\n", ask_size);
    printf("  Timestamp: %s\n", timestamp);

    // Print the local time using the provided function
    print_local_time(timestamp);
    printf("\n");

    // Free the JSON object
    json_decref(root);
}

void parse_trade_data(const char *received_data) {
    json_error_t error;
    json_t *trade = json_loads(received_data, 0, &error);

    if (!trade) {
        printf("Error parsing JSON: %s\n", error.text);
        return;
    }

    printf("JSON data before parsing: %s\n", received_data);

    // Check if message type is 't'
    json_t *msg_type = json_object_get(trade, "T");
    if (!msg_type || !json_is_string(msg_type) || strcmp(json_string_value(msg_type), "t") != 0) {
        json_decref(trade);
        return;
    }

    json_t *symbol = json_object_get(trade, "S");
    json_t *trade_id = json_object_get(trade, "i");
    json_t *exchange = json_object_get(trade, "x");
    json_t *price = json_object_get(trade, "p");
    json_t *size = json_object_get(trade, "s");
    json_t *trade_conditions = json_object_get(trade, "c");
    json_t *tape = json_object_get(trade, "z");
    json_t *timestamp = json_object_get(trade, "t");

    printf("Trade data:\n");
    printf("  Symbol: %s\n", json_string_value(symbol));
    printf("  Trade ID: %lld\n", json_integer_value(trade_id));
    printf("  Exchange: %s\n", json_string_value(exchange));
    printf("  Price: %.4f\n", json_real_value(price));
    printf("  Size: %d\n", json_integer_value(size));

    printf("  Trade Conditions: ");
    size_t index;
    json_t *condition;
    json_array_foreach(trade_conditions, index, condition) {
        printf("%s", json_string_value(condition));
    }
    printf("\n");

    printf("  Tape: %s\n", json_string_value(tape));
    printf("  Timestamp: %s\n", json_string_value(timestamp));
    print_local_time(json_string_value(timestamp));
    printf("\n");

    json_decref(trade);
}

void process_received_data(const char *data) {
    printf("Received data: %s\n", (char *)data);

    json_t *root, *element, *message_type;
    json_error_t error;
    size_t index;

    root = json_loads(data, 0, &error);
    if (!root) {
        fprintf(stderr, "Error parsing JSON data: %s\n", error.text);
        return;
    }

    if (json_is_object(root)) {
        json_t *temp = json_array();
        json_array_append_new(temp, root);
        root = temp;
    } else if (!json_is_array(root)) {
        fprintf(stderr, "Error: JSON data is not an object or array.\n");
        json_decref(root);
        return;
    }

    json_array_foreach(root, index, element) {
        message_type = json_object_get(element, "T");
        if (!message_type || !json_is_string(message_type)) {
            fprintf(stderr, "Error: missing or invalid message type in JSON data.\n");
            continue;
        }

        const char *msg_type_str = json_string_value(message_type);
        char *element_str = json_dumps(element, 0);

        if (strcmp(msg_type_str, "t") == 0) {
            parse_trade_data(element_str);
        } else if (strcmp(msg_type_str, "q") == 0) {
            parse_quote_data(element_str);
        } else if (strcmp(msg_type_str, "b") == 0) {
            parse_bar_data(element_str);
        }

        free(element_str);
    }

    json_decref(root);
}

void send_auth_message(struct lws *wsi) {
    char *apca_api_key_id = getenv("APCA_API_KEY_ID");
    char *apca_api_secret_key = getenv("APCA_API_SECRET_KEY");
    if (!apca_api_key_id || !apca_api_secret_key) {
        fprintf(stderr, "Error: APCA_API_KEY_ID and/or APCA_API_SECRET_KEY environment variables not set.\n");
        interrupted = 1;
        return;
    }

    json_t *auth_message_json = json_object();
    json_object_set_new(auth_message_json, "action", json_string("auth"));
    json_object_set_new(auth_message_json, "key", json_string(apca_api_key_id));
    json_object_set_new(auth_message_json, "secret", json_string(apca_api_secret_key));
    char *auth_message = json_dumps(auth_message_json, JSON_COMPACT);

    if (!auth_message_json) {
        fprintf(stderr, "Error creating json_t object for auth_message_json.\n");
        return;
    }

    unsigned char buf[LWS_PRE + strlen(auth_message)];
    memcpy(&buf[LWS_PRE], auth_message, strlen(auth_message));
    lws_write(wsi, &buf[LWS_PRE], strlen(auth_message), LWS_WRITE_TEXT);

    json_decref(auth_message_json);
    free(auth_message);
}

void send_subscription_message(struct lws *wsi, json_t *params) {
    json_t *subscription_message_json = json_object();
    json_object_set_new(subscription_message_json, "action", json_string("subscribe"));
    if (params) {
        json_t *trades = json_object_get(params, "trades");
        json_t *quotes = json_object_get(params, "quotes");
        json_t *bars = json_object_get(params, "bars");

        if (trades && json_array_size(trades) > 0) {
            json_object_set(subscription_message_json, "trades", trades);
        }
        if (quotes && json_array_size(quotes) > 0) {
            json_object_set(subscription_message_json, "quotes", quotes);
        }
        if (bars && json_array_size(bars) > 0) {
            json_object_set(subscription_message_json, "bars", bars);
        }
    }

    char *subscription_message = json_dumps(subscription_message_json, JSON_COMPACT);

    printf("Sending subscription message: %s\n", subscription_message);

    unsigned char sub_buf[LWS_PRE + strlen(subscription_message)];
    memcpy(&sub_buf[LWS_PRE], subscription_message, strlen(subscription_message));
    lws_write(wsi, &sub_buf[LWS_PRE], strlen(subscription_message), LWS_WRITE_TEXT);

    json_decref(subscription_message_json);
    free(subscription_message);
}

// WebSocket callback function for Alpaca's API
int callback_alpaca( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

  switch (reason) {
    // Connection established
    case LWS_CALLBACK_CLIENT_ESTABLISHED: {
      puts("Connected to Alpaca WebSocket server.");

      // Send the authentication message
      send_auth_message(wsi);

      // Send the subscription message
      send_subscription_message(wsi, (json_t *)user);

      break;
    }
    // Data received
    case LWS_CALLBACK_CLIENT_RECEIVE: {
      // Process the received data
      process_received_data(in);
      break;
    }
    // Connection closed
    case LWS_CALLBACK_CLIENT_CLOSED: {
      puts("Connection closed.");
      interrupted = 1;
      break;
    }
    default:
      break;
  }
  return 0;
}

// Signal handler for SIGINT (e.g., Ctrl+C)
void sigint_handler(int sig) {
  fprintf(stderr, "Received signal %d, terminating...\n", sig);
  interrupted = 1;
}

// Function to convert a string to uppercase
void to_upper(char *str) {
  for (int i = 0; str[i]; i++) {
    str[i] = toupper((unsigned char) str[i]);
  }
}

// Function to parse the input symbols (e.g., trades, quotes, bars)
json_t *parse_symbols(const char *symbols_str) {
  json_t *symbols = json_array();

  if (strcmp(symbols_str, "*") == 0) {
    json_array_append_new(symbols, json_string("*"));
  } else {
    char *str = strdup(symbols_str);
    char *token = strtok(str, ",");

    while (token) {
      to_upper(token);  // Convert the token to upper-case
      json_array_append_new(symbols, json_string(token));
      token = strtok(NULL, ",");
    }

    free(str);
  }

  return symbols;
}

// Function to print the help message with usage instructions
void print_help(const char *program_name) {
  fprintf(stderr, "Usage: %s [-t trades] [-q quotes] [-b bars]\n", program_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -t trades : Comma-separated list of trade symbols, or \"*\" for all trades (with quotes).\n");
  fprintf(stderr, "  -q quotes : Comma-separated list of quote symbols, or \"*\" for all quotes (with quotes).\n");
  fprintf(stderr, "  -b bars   : Comma-separated list of bar symbols, or \"*\" for all bars (with quotes).\n");
  fprintf(stderr, "  -s sip    : Choose the data source. Allowed values are 'sip' (default) or 'iex'.\n");
  fprintf(stderr, "\n");
}
