#!/usr/bin/env bash
set -euo pipefail

make clean && make

echo "Smoke test 1"
./build/ipc_pipes --producers 2 --consumers 2 --messages 10000 --msg-size 32

echo "Smoke test 2 (more contention)"
./build/ipc_pipes --producers 4 --consumers 1 --messages 5000 --msg-size 64

