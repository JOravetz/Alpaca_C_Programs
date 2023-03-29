/*
"alpaca_websocket_jansson.c": This C program connects to Alpaca's WebSocket API and
subscribes to real-time trade, quote, and bar data for specified symbols. It
prints the received data to the console. The program allows the user to choose
between SIP or IEX data source.
The program requires the APCA_API_KEY_ID and APCA_API_SECRET_KEY environment
variables to be set, which are used for authentication.
Usage: alpaca_websocket_jansson [-t trades] [-q quotes] [-b bars] [-s sip]
Options:
-t trades : Comma-separated list of trade symbols, or "*" for all trades (with quotes).
-q quotes : Comma-separated list of quote symbols, or "*" for all quotes (with quotes).
-b bars : Comma-separated list of bar symbols, or "*" for all bars (with quotes).
-s sip : Choose the data source. Allowed values are 'sip' (default) or 'iex'.

To exit the program, press Ctrl+C.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <libwebsockets.h>
#include <unistd.h>
#include <jansson.h>
#include <getopt.h>
#include "alpaca_lib_jansson.h"  // Include the header file for the library

extern int interrupted;

// WebSocket protocols
static struct lws_protocols protocols[] = {
    {"alpaca", callback_alpaca, 0, 0},
    {NULL, NULL, 0, 0}};

int main(int argc, char *argv[]) {
    json_t *params = json_object();
    int opt;

    // If no command-line arguments are provided, print the help message and exit
    if (argc == 1) {
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Default WebSocket path for SIP data source
    char *path = "/v2/sip";

    // Parse the command-line options
    while ((opt = getopt(argc, argv, "t:q:b:s:")) != -1) {
        switch (opt) {
            case 't':
                json_object_set_new(params, "trades", parse_symbols(optarg));
                break;
            case 'q':
                json_object_set_new(params, "quotes", parse_symbols(optarg));
                break;
            case 'b':
                json_object_set_new(params, "bars", parse_symbols(optarg));
                break;
            case 's':
                if (strcmp(optarg, "sip") == 0) {
                    path = "/v2/sip";
                } else if (strcmp(optarg, "iex") == 0) {
                    path = "/v2/iex";
                } else {
                    fprintf(stderr, "Invalid value for -s option. Allowed values are 'sip' or 'iex'.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                print_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Set the SIGINT signal handler
    signal(SIGINT, sigint_handler);

    // Create a WebSocket context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.fd_limit_per_thread = 1;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Error creating WebSocket context.\n");
        return -1;
    }

    // Set up the connection information
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = "stream.data.alpaca.markets";
    ccinfo.port = 443;
    ccinfo.path = path;
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = "alpaca";
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
    ccinfo.userdata = params;

    // Connect to the WebSocket server
    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        fprintf(stderr, "Error connecting to WebSocket server.\n");
        lws_context_destroy(context);
        return -1;
    }

    // Main event loop: process WebSocket events until interrupted
    while (!interrupted) {
        lws_service(context, 50);
    }

    // Clean up: destroy the WebSocket context and delete the JSON object
    lws_context_destroy(context);
    json_decref(params);

    // Exit the program
    return 0;
}
