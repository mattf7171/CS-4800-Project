#!/usr/bin/env bash
set -euo pipefail

make -s clean && make -s

OUT="docs/bench_m1.txt"
: > "$OUT"

run_case () {
  echo ">>> $*" | tee -a "$OUT"
  ./build/ipc_pipes "$@" | tee -a "$OUT"
  echo | tee -a "$OUT"
}

run_case --producers 1 --consumers 1 --messages 20000 --msg-size 64
run_case --producers 2 --consumers 1 --messages 20000 --msg-size 64
run_case --producers 4 --consumers 1 --messages 20000 --msg-size 64
run_case --producers 4 --consumers 2 --messages 20000 --msg-size 64

echo "Saved benchmark output to $OUT"

