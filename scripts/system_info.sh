#!/usr/bin/env bash
set -euo pipefail

mkdir -p docs

OUT="docs/system_info.txt"
: > "$OUT"

echo "=== Date ===" | tee -a "$OUT"
date | tee -a "$OUT"
echo | tee -a "$OUT"

echo "=== uname -a ===" | tee -a "$OUT"
uname -a | tee -a "$OUT"
echo | tee -a "$OUT"

echo "=== CPU (lscpu) ===" | tee -a "$OUT"
lscpu | tee -a "$OUT"
echo | tee -a "$OUT"

echo "=== Memory (free -h) ===" | tee -a "$OUT"
free -h | tee -a "$OUT"
echo | tee -a "$OUT"

echo "=== mqueue limits ===" | tee -a "$OUT"
echo -n "msg_max: " | tee -a "$OUT"
cat /proc/sys/fs/mqueue/msg_max | tee -a "$OUT"
echo -n "msgsize_max: " | tee -a "$OUT"
cat /proc/sys/fs/mqueue/msgsize_max | tee -a "$OUT"
echo | tee -a "$OUT"

echo "Saved system info to $OUT"