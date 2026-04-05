# Final Report Outline (Draft)

1. Project Overview
   - Producer–consumer problem
   - Why IPC comparisons matter
   - Implementations included

2. IPC Implementations
   - Pipes: design + message framing + PIPE_BUF constraint
   - Shared memory + semaphores: bounded buffer + empty/full/mutex
   - POSIX message queues: mq limits and behavior

3. Correctness Strategy
   - What can go wrong (race conditions, interleaving, deadlocks)
   - How I validated correctness (counters, scripts, PASS/FAIL gating)

4. Benchmark Methodology
   - Workload matrix design
   - Machine info and OS limits (system_info.txt)
   - How results were collected

5. Results
   - Summary table (from CSV)
   - Selected charts for key comparisons

6. Analysis
   - Explain performance trade-offs and scaling behavior
   - Notes on OS limits impacting MQ (maxmsg)

7. Conclusion
   - What I learned
   - What approach fits what scenario