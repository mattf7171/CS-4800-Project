#!/usr/bin/env bash
set -euo pipefail

make clean && make

echo "== M2 Smoke: shared memory + semaphores (2P/2C) =="
./build/ipc_shm_sem --producers 2 --consumers 2 --messages 10000 --msg-size 64 --slots 64

echo
echo "== M2 Smoke: contention case (4P/1C) =="
./build/ipc_shm_sem --producers 4 --consumers 1 --messages 5000 --msg-size 64 --slots 32