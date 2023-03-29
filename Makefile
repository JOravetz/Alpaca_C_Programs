CC = gcc
CFLAGS = -w -O3
LIB_NAME = libalpaca_jansson.a
PROGRAM_NAME = alpaca_websocket_jansson
OBJS = alpaca_lib_jansson.o
LIBS = -lwebsockets -ljansson
AR = ar
ARFLAGS = rcs

all: $(PROGRAM_NAME)

$(PROGRAM_NAME): $(LIB_NAME) $(PROGRAM_NAME).c
	$(CC) $(CFLAGS) -o $@ $(PROGRAM_NAME).c -L. -lalpaca_jansson $(LIBS)

$(LIB_NAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

alpaca_lib_jansson.o: alpaca_lib_jansson.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(PROGRAM_NAME) $(LIB_NAME) $(OBJS)

.PHONY: all clean
