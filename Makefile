CC=gcc
CFLAGS=-O2 -Wall -Wextra -Iinclude
LDFLAGS=

BIN=build/ipc_pipes
SRC=src/main.c src/producer.c src/consumer.c src/util.c

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -rf build

.PHONY: all clean

