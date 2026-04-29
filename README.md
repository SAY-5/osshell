# osshell

C++20 Unix shell with a built-in preemptive process scheduler
simulator. Tokenizer, parser, fork/exec runner with pipes +
redirection + job control, and a side-by-side MLFQ vs round-robin
benchmark that demonstrates a 40-60% reduction in context switches
on bursty mixed workloads.

```
input ──tokenizer──▶ tokens ──parser──▶ Pipeline ──Runner──▶ fork/exec/dup2
                                                  │
                                                  ├─▶ JobTable (running/stopped/done)
                                                  │       │
                                                  │       └─▶ SSE Feed (event:job)
                                                  │
                                                  └─▶ schedbench: MLFQ vs RR
```

## Versions

| Version | Capability | Status |
|---|---|---|
| v1 | Tokenizer (quotes, escapes, env-var expansion) + parser (pipes, redirects, background, semicolons) + fork/exec runner with pipe wiring + job table | shipped |
| v2 | SSE-style Feed emits `event: job` frames on every state transition; `event: sched` frames carry scheduler-simulator events | shipped |
| v3 | Preemptive scheduler simulator: round-robin baseline + MLFQ (4 levels, quantum doubling, demote-on-quantum / promote-on-IO). Headline benchmark (`schedbench`) shows ≥40% context-switch reduction on the canonical mixed workload | shipped |

## Headline result

```
$ ./build/schedbench 4
workload: mixed-bursty (2 cpu-bound + 6 interactive)
base_quantum_ms: 4

policy        wall_ms  ctx_sw  idle_ms
round_robin       490     136        0
mlfq              490      54        0

context-switch reduction: 60.3% (lower is better)
```

## Quickstart

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # 31 tests, 6 binaries
./build/osh                                   # interactive shell
./build/schedbench 4                          # MLFQ vs RR benchmark
```

## Tests

31 tests across 6 binaries:
- **tokenizer** (8): word splitting, operators, single/double quotes, env expansion, escapes, error path
- **parser** (7): single command, pipeline of N, redirect attachment, background, semicolon split, error paths
- **executor** (3): plan extraction without forking
- **jobs** (4): unique IDs, state transitions, clear-done, event hook
- **scheduler** (5): both policies complete, MLFQ has ≥30% fewer switches, I/O blocking, empty workload
- **stream** (4): SSE frame format for both job + sched events, drain idempotent

## Why a simulator instead of measuring real Linux

Measuring real kernel context-switch overhead requires `perf` /
ftrace integration that's wildly different across distros and
kernel versions. The simulator gives a reproducible, deterministic
comparison that anyone can run in CI in milliseconds, and the
relative result (MLFQ vs RR on the same workload) holds in real
kernels where MLFQ-like priority schemes (CFS, Windows priority
boosting) reduce switches for the same reason: interactive tasks
get longer effective quanta at the top level once CPU-bound peers
are demoted.

See `ARCHITECTURE.md` for the full design.
