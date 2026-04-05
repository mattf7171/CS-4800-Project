# PR4 Analysis Notes (Draft)

## What I benchmarked
- Compared three IPC approaches:
  - pipes (`ipc_pipes`)
  - shared memory + semaphores (`ipc_shm_sem`)
  - POSIX message queues (`ipc_mq`)
- Ran a workload matrix varying:
  - producers (1, 2, 4)
  - consumers (1, 2)
  - message size (64, 256)
  - constant total messages per run (200000 total, divided across producers)
- Saved results:
  - docs/bench_pr4_results.csv
  - docs/bench_pr4_raw.txt
  - docs/system_info.txt (machine info + kernel + mqueue limits)

## Correctness validation approach
- For each run, I checked the output for any nonzero:
  - dup
  - out_of_range
  - malformed
- The benchmark script marks each run PASS/FAIL in the CSV.

## Early observations / hypotheses (to verify in final report)
- Pipes tends to be fastest for small messages due to low overhead, but requires careful message framing for multi-producer safety.
- Shared memory + semaphores has overhead from synchronization and critical sections, but should scale well with larger payloads if buffer sizing is tuned.
- Message queues preserve message boundaries (less framing risk), but performance depends heavily on kernel limits (mq_maxmsg) and queue contention.

## Next step before final submission
- Pick a smaller subset of the most “telling” workloads for charts and discussion.
- Turn CSV results into a simple table/graph for the final report.
- Write a “why” explanation for performance trade-offs, not just numbers.