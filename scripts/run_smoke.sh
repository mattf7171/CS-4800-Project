#!/usr/bin/env bash
set -euo pipefail

make clean && make

echo "== Smoke test: 2 producers, 2 consumers =="
./build/ipc_pipes --producers 2 --consumers 2 --messages 10000 --msg-size 64

echo
echo "== Smoke test: 4 producers, 1 consumer (contention) =="
./build/ipc_pipes --producers 4 --consumers 1 --messages 5000 --msg-size 64

