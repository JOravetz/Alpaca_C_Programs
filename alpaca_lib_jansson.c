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
#include <stdbool.h>

#define MAX_STORED_BARS 1000
#define MAX_STORED_TRADES 1000
#define MAX_STORED_QUOTE_PRICES 1000

int interrupted = 0;

typedef struct Bar {
    char *symbol;
    double open;
    double high;
    double low;
    double close;
    double vw;
    int volume;
    int trades;
    char *timestamp_str;
    char *local_time_str;
    double digital_seconds;
    struct Bar *next;
} Bar;

typedef struct Trade {
    const char *symbol;
    long long trade_id;
    const char *exchange;
    double price;
    int size;
    char **trade_conditions;
    size_t num_conditions;
    const char *tape;
    const char *timestamp_str;
    const char *local_time_str;
    double digital_seconds;
    struct Trade *next;
} Trade;

typedef struct Quote {
    char *symbol;
    char *bid_exchange;
    double bid_price;
    int bid_size;
    char *ask_exchange;
    double ask_price;
    int ask_size;
    char *timestamp_str;
    char *local_time_str;
    double digital_seconds;
    struct Quote *next;
} Quote;

Bar *bar_list_head = NULL;
Trade *trade_list_head = NULL;
Quote *quote_list_head = NULL;

Bar *create_bar_node(const char *symbol, double open, double high, double low, double close, double vw, int volume, int trades, const char *timestamp_str, const char *local_time_str, double digital_seconds) {
    Bar *new_node = (Bar *)malloc(sizeof(Bar));
    new_node->symbol = strdup(symbol);
    new_node->open = open;
    new_node->high = high;
    new_node->low = low;
    new_node->close = close;
    new_node->vw = vw;
    new_node->volume = volume;
    new_node->trades = trades;
    new_node->timestamp_str = strdup(timestamp_str);
    new_node->local_time_str = strdup(local_time_str);
    new_node->digital_seconds = digital_seconds;
    new_node->next = NULL;
    return new_node;
}

Trade *create_trade_node(const char *symbol, long long trade_id, const char *exchange, double price, int size, json_t *trade_conditions, const char *timestamp_str, const char *local_time_str, double digital_seconds, const char *tape) {
    Trade *new_node = (Trade *)malloc(sizeof(Trade));
    new_node->symbol = strdup(symbol);
    new_node->trade_id = trade_id;
    new_node->exchange = strdup(exchange);
    new_node->price = price;
    new_node->size = size;

    size_t num_conditions = json_array_size(trade_conditions);
    new_node->trade_conditions = (char **)malloc(num_conditions * sizeof(char *));
    for (size_t i = 0; i < num_conditions; ++i) {
        new_node->trade_conditions[i] = strdup(json_string_value(json_array_get(trade_conditions, i)));
    }
    new_node->num_conditions = num_conditions;

    new_node->tape = strdup(tape);
    new_node->timestamp_str = strdup(timestamp_str);
    new_node->local_time_str = strdup(local_time_str);
    new_node->digital_seconds = digital_seconds;
    new_node->next = NULL;
    return new_node;
}

Quote *create_quote_node(const char *symbol, const char *bid_exchange, double bid_price, int bid_size, const char *ask_exchange, double ask_price, int ask_size, const char *timestamp_str, const char *local_time_str, double digital_seconds) {
    Quote *new_node = (Quote *)malloc(sizeof(Quote));
    new_node->symbol = strdup(symbol);
    new_node->bid_exchange = strdup(bid_exchange);
    new_node->bid_price = bid_price;
    new_node->bid_size = bid_size;
    new_node->ask_exchange = strdup(ask_exchange);
    new_node->ask_price = ask_price;
    new_node->ask_size = ask_size;
    new_node->timestamp_str = strdup(timestamp_str);
    new_node->local_time_str = strdup(local_time_str);
    new_node->digital_seconds = digital_seconds;
    new_node->next = NULL;
    return new_node;
}

void insert_bar_node(Bar **head, Bar *new_node) {
    new_node->next = *head;
    *head = new_node;
}

void insert_trade_node(Trade **head, Trade *new_node) {
    new_node->next = *head;
    *head = new_node;
}

void insert_quote_node(Quote **head, Quote *new_node) {
    new_node->next = *head;
    *head = new_node;
}

void remove_oldest_bar(Bar **head) {
    Bar *current = *head;
    Bar *prev = NULL;

    if (current == NULL) {
        return;
    }

    while (current->next != NULL) {
        prev = current;
        current = current->next;
    }

    if (prev) {
        prev->next = NULL;
    } else {
        *head = NULL;
    }

    free(current->symbol);
    free(current->timestamp_str);
    free(current->local_time_str);
    free(current);
}

void remove_oldest_trade(Trade **head) {
    Trade *current = *head;
    Trade *prev = NULL;

    if (current == NULL) {
        return;
    }

    while (current->next != NULL) {
        prev = current;
        current = current->next;
    }

    if (prev) {
        prev->next = NULL;
    } else {
        *head = NULL;
    }

    for (int i = 0; i < current->num_conditions; ++i) {
        free(current->trade_conditions[i]);
    }
    free(current->trade_conditions);
    free(current->symbol);
    free(current->exchange);
    free(current->tape);
    free(current->timestamp_str);
    free(current->local_time_str);
    free(current);
}

void remove_oldest_quote(Quote **head) {
    Quote *current = *head;
    Quote *prev = NULL;

    if (current == NULL) {
        return;
    }

    while (current->next != NULL) {
        prev = current;
        current = current->next;
    }

    if (prev) {
        prev->next = NULL;
    } else {
        *head = NULL;
    }

    free(current->symbol);
    free(current->bid_exchange);
    free(current->ask_exchange);
    free(current->timestamp_str);
    free(current->local_time_str);
    free(current);
}

double time_string_to_seconds_since_1970(const char *time_string) {
    struct tm tm_time = {0};
    time_t epoch_time;

    // Parse the time string
    if (strptime(time_string, "%Y-%m-%d %H:%M:%S %Z", &tm_time) == NULL) {
        fprintf(stderr, "Error: failed to parse time string.\n");
        return -1;
    }

    // Convert struct tm to time_t (seconds since 1970)
    epoch_time = mktime(&tm_time);
    if (epoch_time == -1) {
        fprintf(stderr, "Error: failed to convert struct tm to time_t.\n");
        return -1;
    }

    // Return the result as a double
    return (double)epoch_time;
}

char *print_local_time(const char *timestamp) {
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

    char *buffer = (char *)malloc(64 * sizeof(char));
    strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", local_tm);

    char *local_time_zone = (char *)malloc(8 * sizeof(char));
    strftime(local_time_zone, 8, "%Z", local_tm);

    strcat(buffer, " ");
    strcat(buffer, local_time_zone);

    // Free the memory allocated for local_time_zone
    free(local_time_zone);

    return buffer;
}

void extract_bar_close_prices_by_symbol(Bar *head, const char *symbol, double **prices, size_t *num_prices) {
    size_t count = 0;

    // Count the number of bars with the specified symbol
    Bar *current = head;
    while (current) {
        if (strcmp(current->symbol, symbol) == 0 && current->close != 0) {
            count++;
        }
        current = current->next;
    }

    // Allocate memory for the close prices array
    *prices = (double *)malloc(count * sizeof(double));
    *num_prices = count;

    // Extract the close prices in reverse order
    size_t index = count;
    current = head;
    while (current && index > 0) {
        if (strcmp(current->symbol, symbol) == 0 && current->close != 0) {
            (*prices)[--index] = current->close;
        }
        current = current->next;
    }
}

double* extract_trade_prices_by_symbol(const Trade* trade_list_head, const char* symbol, size_t* num_prices) {
    // Count the number of trades for the specified symbol
    size_t trade_count = 0;
    const Trade* current = trade_list_head;
    while (current) {
        if (strcmp(current->symbol, symbol) == 0) {
            trade_count++;
        }
        current = current->next;
    }

    if (trade_count == 0) {
        *num_prices = 0;
        return NULL;
    }

    double* prices = (double*)malloc(trade_count * sizeof(double));
    if (prices == NULL) {
        *num_prices = 0;
        return NULL;
    }

    // Extract the prices for the specified symbol in reverse order
    size_t price_index = trade_count - 1;
    current = trade_list_head;
    while (current) {
        if (strcmp(current->symbol, symbol) == 0 && current->price != 0) {
            prices[price_index--] = current->price;
        }
        current = current->next;
    }

    *num_prices = trade_count - price_index - 2;
    return prices + price_index + 1;
}

void extract_bid_ask_prices_by_symbol(Quote *quote_list_head, const char *symbol, double **bid_prices, size_t *num_bid_prices, double **ask_prices, size_t *num_ask_prices) {
    *num_bid_prices = 0;
    *num_ask_prices = 0;

    // Count the number of bid and ask prices for the given symbol
    Quote *current = quote_list_head;
    while (current) {
        if (strcmp(current->symbol, symbol) == 0) {
            if (current->bid_price != 0) {
                (*num_bid_prices)++;
            }
            if (current->ask_price != 0) {
                (*num_ask_prices)++;
            }
        }
        current = current->next;
    }

    // Allocate memory for the bid and ask price arrays
    *bid_prices = (double *)malloc(*num_bid_prices * sizeof(double));
    *ask_prices = (double *)malloc(*num_ask_prices * sizeof(double));

    // Fill the bid and ask price arrays with the corresponding prices
    size_t bid_index = 0;
    size_t ask_index = 0;
    current = quote_list_head;
    while (current) {
        if (strcmp(current->symbol, symbol) == 0) {
            if (current->bid_price != 0) {
                (*bid_prices)[(*num_bid_prices) - 1 - bid_index++] = current->bid_price;
            }
            if (current->ask_price != 0) {
                (*ask_prices)[(*num_ask_prices) - 1 - ask_index++] = current->ask_price;
            }
        }
        current = current->next;
    }
}

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
    const char *msg_type, *symbol, *timestamp_str;
    double open, high, low, close, vw;
    int volume, trades;

    msg_type = json_string_value(json_object_get(root, "T"));
    symbol = json_string_value(json_object_get(root, "S"));
    open = json_number_value(json_object_get(root, "o"));
    high = json_number_value(json_object_get(root, "h"));
    low = json_number_value(json_object_get(root, "l"));
    close = json_number_value(json_object_get(root, "c"));
    volume = json_integer_value(json_object_get(root, "v"));
    timestamp_str = json_string_value(json_object_get(root, "t"));
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
    printf("  Timestamp: %s\n", timestamp_str);
    printf("  Number of Trades: %d\n", trades);
    printf("  VWAP: %.5f\n", vw);

    // Print the local time using the provided function
    char *local_time_str = print_local_time(timestamp_str);
    printf("\n");

    double digital_seconds = time_string_to_seconds_since_1970(local_time_str);

    // Create and insert the new bar node with digital_seconds
    Bar *new_node = create_bar_node(symbol, open, high, low, close, vw, volume, trades, timestamp_str, local_time_str, digital_seconds);
    insert_bar_node(&bar_list_head, new_node);

    // Limit the number of stored bars
    int bar_count = 0;
    Bar *current = bar_list_head;
    while (current) {
        bar_count++;
        if (bar_count > MAX_STORED_BARS) {
            remove_oldest_bar(&bar_list_head);
        }
        current = current->next;
    }

    // Extract and print close prices for the parsed symbol
    double *close_prices;
    size_t num_close_prices;
    extract_bar_close_prices_by_symbol(bar_list_head, symbol, &close_prices, &num_close_prices);

    printf("Close prices for %s:\n", symbol);
    for (size_t i = 0; i < num_close_prices; ++i) {
        printf("Index %zu - Close Price: %.4f\n", i, close_prices[i]);
    }
    printf("\n");

    // Free the allocated memory
    free(close_prices);
}

void parse_trade_data(const char *received_data) {
    json_error_t error;
    json_t *root = json_loads(received_data, 0, &error);

    if (!root) {
        printf("Error parsing JSON: %s\n", error.text);
        return;
    }

    if (!json_is_object(root)) {
        fprintf(stderr, "Error: JSON data is not an object.\n");
        json_decref(root);
        return;
    }

    printf("JSON data before parsing: %s\n", received_data);

    const char *msg_type, *symbol, *exchange, *timestamp_str, *tape;
    long long trade_id;
    double price;
    int size;
    json_t *trade_conditions;

    msg_type = json_string_value(json_object_get(root, "T"));
    symbol = json_string_value(json_object_get(root, "S"));
    trade_id = json_integer_value(json_object_get(root, "i"));
    exchange = json_string_value(json_object_get(root, "x"));
    price = json_real_value(json_object_get(root, "p"));
    size = json_integer_value(json_object_get(root, "s"));
    trade_conditions = json_object_get(root, "c");
    tape = json_object_get(root, "z");
    timestamp_str = json_string_value(json_object_get(root, "t"));

    printf("Trade data:\n");
    printf("  Message Type: %s\n", msg_type);
    printf("  Symbol: %s\n", symbol);
    printf("  Trade ID: %lld\n", trade_id);
    printf("  Exchange: %s\n", exchange);
    printf("  Price: %.4f\n", price);
    printf("  Size: %d\n", size);

    printf("  Trade Conditions: ");
    size_t index;
    json_t *condition;
    json_array_foreach(trade_conditions, index, condition) {
        printf("%s", json_string_value(condition));
    }
    printf("\n");

    printf("  Tape: %s\n", tape);
    printf("  Timestamp: %s\n", timestamp_str);
    char *local_time_str = print_local_time(timestamp_str);
    printf("\n");

    double digital_seconds = time_string_to_seconds_since_1970(local_time_str);

    // Create and insert the new trade node with digital_seconds
    Trade *new_node = create_trade_node(symbol, trade_id, exchange, price, size, trade_conditions, timestamp_str, local_time_str, digital_seconds, tape);
    insert_trade_node(&trade_list_head, new_node);

    // Limit the number of stored trades
    int trade_count = 0;
    Trade *current = trade_list_head;
    while (current) {
        trade_count++;
        if (trade_count > MAX_STORED_TRADES) {
            remove_oldest_trade(&trade_list_head);
        }
        current = current->next;
    }

    // Extract and print trade prices for the parsed symbol
    size_t num_prices;
    double* prices = extract_trade_prices_by_symbol(trade_list_head, symbol, &num_prices);

    if (prices != NULL) {
        printf("Trade prices for %s:\n", symbol);
        double prev_price = -1.0;
        for (size_t i = 0; i < num_prices; ++i) {
            if (i == 0 || prices[i] != prev_price) {
                printf("Index %zu: %.4f\n", i, prices[i]);
            }
            prev_price = prices[i];
        }
        free(prices);
    } else {
        printf("No trade data found for %s.\n", symbol);
    }

    // Free the JSON object
    json_decref(root);
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

    char *local_time = print_local_time(timestamp);
    double digital_seconds = time_string_to_seconds_since_1970(local_time);

    // Create a new quote node and insert it into the list
    Quote *new_node = create_quote_node(symbol, bid_exchange, bid_price, bid_size, ask_exchange, ask_price, ask_size, timestamp, local_time, digital_seconds);
    insert_quote_node(&quote_list_head, new_node);

    // Remove the oldest quote node if the list is too long
    int quote_count = 0;
    Quote *current = quote_list_head;
    while (current != NULL) {
        quote_count++;
        current = current->next;
    }
    if (quote_count > MAX_STORED_QUOTE_PRICES) {
        remove_oldest_quote(&quote_list_head);
    }

    // Extract and print bid and ask prices for the parsed symbol
    double *bid_prices, *ask_prices;
    size_t num_bid_prices, num_ask_prices;
    extract_bid_ask_prices_by_symbol(quote_list_head, symbol, &bid_prices, &num_bid_prices, &ask_prices, &num_ask_prices);

    printf("Bid and Ask prices for %s:\n", symbol);
    size_t max_length = num_bid_prices < num_ask_prices ? num_bid_prices : num_ask_prices;
    double prev_bid_price = 0.0;
    double prev_ask_price = 0.0;
    for (size_t i = 0; i < max_length; ++i) {
        if (bid_prices[i] != prev_bid_price || ask_prices[i] != prev_ask_price) {
            printf("Index %zu - Bid Price: %.4f Ask Price: %.4f\n", i, bid_prices[i], ask_prices[i]);
            prev_bid_price = bid_prices[i];
            prev_ask_price = ask_prices[i];
        }
    }

    // Free the allocated memory
    free(bid_prices);
    free(ask_prices);

    // Free the JSON object
    json_decref(root);
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

void free_bar_list(Bar *head) {
    Bar *current = head;
    Bar *next;

    while (current) {
        next = current->next;
        free(current->symbol);
        free(current);
        current = next;
    }
}

// Signal handler for SIGINT (e.g., Ctrl+C)
void sigint_handler(int sig) {
  fprintf(stderr, "Received signal %d, terminating...\n", sig);
  free_bar_list(bar_list_head);
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
