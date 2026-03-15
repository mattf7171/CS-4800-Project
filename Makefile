CC=gcc
CFLAGS=-O2 -Wall -Wextra -Iinclude
LDFLAGS=-pthread

BIN_PIPES=build/ipc_pipes
BIN_SHM=build/ipc_shm_sem
BIN_MQ=build/ipc_mq

SRC_PIPES=src/main.c src/producer.c src/consumer.c src/util.c
SRC_SHM=src/shm_sem_main.c
SRC_MQ=src/mq_main.c

all: $(BIN_PIPES) $(BIN_SHM) $(BIN_MQ)

$(BIN_PIPES): $(SRC_PIPES)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC_PIPES) $(LDFLAGS)

$(BIN_SHM): $(SRC_SHM)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC_SHM) $(LDFLAGS)

$(BIN_MQ): $(SRC_MQ)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC_MQ) $(LDFLAGS) -lrt

clean:
	rm -rf build

.PHONY: all clean