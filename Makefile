CC = gcc
CFLAGS = -O2 -pipe -std=c11 -Wall -Iinclude
LDFLAGS = -lmicrohttpd -lhiredis -lpthread

SRC = src/main.c src/limiter.c src/store_mem.c src/store_redis.c src/metrics.c
OBJ = $(SRC:.c=.o)
BIN = ratelimiter

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
