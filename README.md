# CS 4800 – IPC & Synchronization Project (Producer–Consumer)

This repository contains my CS 4800 independent project focused on Linux systems programming.  
The project implements the classic **producer–consumer** pattern using multiple **IPC (Inter-Process Communication)** mechanisms and compares them based on **correctness** and **performance**.

✅ **Milestone 1 (current):** Pipes-based baseline implementation  
🔜 Later milestones: POSIX shared memory + semaphores, and a third IPC approach (e.g., message queues or mmap ring buffer)

---

## Project Goals (High Level)
- Implement multiple producer–consumer systems using **different IPC approaches**
- Ensure correctness under concurrency:
  - prevent/identify race conditions
  - avoid deadlocks
  - validate message integrity (no lost/duplicate/out-of-range messages)
- Benchmark under different workloads and analyze trade-offs (not just raw timings)

---

## Milestone 1: Pipes Baseline (What’s included)
The pipes baseline uses:
- **Multiple producer processes**
- **Multiple consumer processes**
- A shared pipe for communication
- A simple message format (header + payload) and validation counters

### Message framing correctness note (important)
With multiple producers writing to the same pipe, the OS guarantees atomicity **per write** only (up to `PIPE_BUF`, typically 4096 bytes).  
If a “logical message” is split across multiple writes (ex: header write + payload write), different producers can interleave writes and corrupt message boundaries.

**Fix used in this project:** each producer writes a complete message (`header + payload`) using a **single write**, and the program rejects configurations where total message size exceeds `PIPE_BUF`.

This prevents interleaving and keeps the consumer aligned.

---

## Repository Layout
- `src/` – C source code
- `include/` – headers
- `scripts/` – repeatable run/benchmark scripts
- `docs/` – notes and saved benchmark output
- `build/` – compiled binaries (generated)

---

## Build
Requirements:
- Linux
- `gcc` and `make`

Compile:
```bash
make clean && make


The binary will be created at:

./build/ipc_pipes


Run

Example (moderate contention):

./build/ipc_pipes --producers 4 --consumers 1 --messages 5000 --msg-size 64


CLI Options

--producers N
Number of producer processes

--consumers N
Number of consumer processes

--messages M
Messages per producer

--msg-size BYTES
Payload size per message (bytes).
Note: sizeof(header) + msg-size must be ≤ PIPE_BUF for safe atomic writes.

--verbose
Print additional debug information


Output (What to expect)

Consumers print per-process stats:

received – number of messages parsed successfully

dup – duplicate message IDs seen

out_of_range – message IDs outside expected ranges

malformed – invalid messages (ex: bad payload length)

Example:

consumer[0]: received=20000 dup=0 out_of_range=0 malformed=0
run: producers=4 consumers=1 messages_per_producer=5000 msg_size=64
timing: 0.035 sec | approx 574670 msgs/sec


Scripts
Smoke tests

Runs a couple correctness-focused configurations:

./scripts/run_smoke.sh

Simple benchmark runner

Runs a few workload configurations and saves output to docs/bench_m1.txt:

./scripts/run_bench.sh


Documentation / Notes

docs/design_notes.md contains milestone notes, including the pipe framing issue and the fix.

Roadmap (Next Milestones)

Milestone 2: Implement bounded buffer using POSIX shared memory + semaphores, with strong correctness testing
