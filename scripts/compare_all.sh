#!/usr/bin/env bash
set -euo pipefail

make -s clean && make -s

OUT="docs/bench_m3.txt"
: > "$OUT"

run_one () {
  label="$1"; shift
  echo ">>> $label $*" | tee -a "$OUT"
  "$@" | tee -a "$OUT"
  echo | tee -a "$OUT"
}

# Same workload across implementations
P=4
C=1
TOTAL=80000   # total messages across all producers
S=64          # payload size

PER_PROD=$((TOTAL / P))

run_one "pipes"   ./build/ipc_pipes   --producers $P --consumers $C --messages $PER_PROD --msg-size $S
run_one "shm_sem" ./build/ipc_shm_sem --producers $P --consumers $C --messages $PER_PROD --msg-size $S --slots 64
run_one "mq"      ./build/ipc_mq      --producers $P --consumers $C --messages $PER_PROD --msg-size $S --maxmsg 10

echo "Saved comparison output to $OUT"