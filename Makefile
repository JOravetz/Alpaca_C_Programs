CC = gcc
CFLAGS = -w -O3
LIB_NAME = libalpaca_jansson.a
PROGRAM_NAME = alpaca_websocket_jansson
PROGRAM_NAME_1 = alpaca_current_price_fetcher_jansson
PROGRAM_NAME_2 = alpaca_memory_price_fetcher
OBJS = alpaca_lib_jansson.o
LIBS = -lwebsockets -ljansson -lcurl
LIBS_NO_WEBSOCKETS = -ljansson -lcurl
AR = ar
ARFLAGS = rcs

all: $(PROGRAM_NAME) $(PROGRAM_NAME_1) $(PROGRAM_NAME_2)

$(PROGRAM_NAME): $(LIB_NAME) $(PROGRAM_NAME).c
	$(CC) $(CFLAGS) -o $@ $(PROGRAM_NAME).c -L. -lalpaca_jansson $(LIBS)

$(PROGRAM_NAME_1): $(LIB_NAME) $(PROGRAM_NAME_1).c
	$(CC) $(CFLAGS) -o $@ $(PROGRAM_NAME_1).c -L. -lalpaca_jansson $(LIBS_NO_WEBSOCKETS)

$(PROGRAM_NAME_2): $(LIB_NAME) $(PROGRAM_NAME_2).c
	$(CC) $(CFLAGS) -o $@ $(PROGRAM_NAME_2).c -L. -lalpaca_jansson $(LIBS_NO_WEBSOCKETS)

$(LIB_NAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

alpaca_lib_jansson.o: alpaca_lib_jansson.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(PROGRAM_NAME) $(PROGRAM_NAME_1) $(PROGRAM_NAME_2) $(LIB_NAME) $(OBJS)

.PHONY: all clean
