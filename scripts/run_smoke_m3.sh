#!/usr/bin/env bash
set -euo pipefail

make clean && make

echo "== M3 Smoke: POSIX message queue (2P/2C) =="
./build/ipc_mq --producers 2 --consumers 2 --messages 10000 --msg-size 64 --maxmsg 10

echo
echo "== M3 Smoke: POSIX message queue contention (4P/1C) =="
./build/ipc_mq --producers 4 --consumers 1 --messages 5000 --msg-size 64 --maxmsg 10