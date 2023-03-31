#ifndef ALPACA_LIB_JANSSON_H
#define ALPACA_LIB_JANSSON_H

#include <jansson.h>
#include <libwebsockets.h>
#include <time.h>

void parse_bar_data(const char *json_data);
void parse_quote_data(const char *json_data);
void parse_trade_data(const char *received_data);
void process_received_data(const char *data);
void send_auth_message(struct lws *wsi);
void send_subscription_message(struct lws *wsi, json_t *params);
int callback_alpaca(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
void sigint_handler(int sig);
void to_upper(char *str);
json_t *parse_symbols(const char *symbols_str);
void print_help(const char *program_name);

#endif // ALPACA_LIB_JANSSON_H
