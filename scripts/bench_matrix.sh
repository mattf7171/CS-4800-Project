#!/usr/bin/env bash
set -euo pipefail

mkdir -p docs
make -s clean && make -s

RAW="docs/bench_pr4_raw.txt"
CSV="docs/bench_pr4_results.csv"

: > "$RAW"
echo "impl,producers,consumers,messages_per_producer,msg_size,extra,seconds,msgs_per_sec,correctness" > "$CSV"

run_case () {
  impl="$1"; shift
  p="$1"; c="$2"; m="$3"; s="$4"; extra="$5"; shift 5
  cmd=("$@")

  echo ">>> [$impl] P=$p C=$c M=$m S=$s extra=$extra" | tee -a "$RAW"
  printf "CMD: %s\n" "${cmd[*]}" | tee -a "$RAW"

  # Run and capture output
  out="$("${cmd[@]}" 2>&1 | tee -a "$RAW")"

  # correctness gate: if any dup/out_of_range/malformed show nonzero => FAIL
  if echo "$out" | grep -Eq 'dup=[1-9]|out_of_range=[1-9]|malformed=[1-9]'; then
    ok="FAIL"
  else
    ok="PASS"
  fi

  # Extract timing and msgs/sec from the timing line
  # expected format: timing: 0.012 sec | approx 12345 msgs/sec
  sec="$(echo "$out" | grep -E 'timing:' | tail -n 1 | awk '{print $2}')"
  mps="$(echo "$out" | grep -E 'timing:' | tail -n 1 | awk '{print $6}')"

  # Fallback if parsing fails
  sec="${sec:-NA}"
  mps="${mps:-NA}"

  echo "${impl},${p},${c},${m},${s},${extra},${sec},${mps},${ok}" >> "$CSV"
  echo | tee -a "$RAW"
}

# Workloads: these are big enough to reduce timing noise but still reasonable.
# Feel free to adjust TOTAL upward if you want more stable numbers.
SIZES=(64 256)
PRODS=(1 2 4)
CONS=(1 2)
TOTAL=200000

for s in "${SIZES[@]}"; do
  for p in "${PRODS[@]}"; do
    for c in "${CONS[@]}"; do
      m=$((TOTAL / p))

      # pipes
      run_case "pipes" "$p" "$c" "$m" "$s" "na" \
        ./build/ipc_pipes --producers "$p" --consumers "$c" --messages "$m" --msg-size "$s"

      # shm/sem: vary slots based on contention; you can tweak these
      slots=64
      if [ "$p" -ge 4 ]; then slots=128; fi
      run_case "shm_sem" "$p" "$c" "$m" "$s" "slots=$slots" \
        ./build/ipc_shm_sem --producers "$p" --consumers "$c" --messages "$m" --msg-size "$s" --slots "$slots"

      # mq: use maxmsg=10 due to kernel defaults (you already discovered this)
      run_case "mq" "$p" "$c" "$m" "$s" "maxmsg=10" \
        ./build/ipc_mq --producers "$p" --consumers "$c" --messages "$m" --msg-size "$s" --maxmsg 10

    done
  done
done

echo "Saved raw output to $RAW"
echo "Saved CSV results to $CSV"