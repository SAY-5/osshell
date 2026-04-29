# Architecture

## Pipeline

```
read line
   │
   ▼
tokenize(line)  ─▶  [Token...]
   │
   ▼
parse(tokens)   ─▶  [Pipeline...]
   │
   ▼
for each pipeline:
   if single-command builtin → in-process
   else                       → Runner.run(pipeline)
                                    │
                                    └─▶ fork * N, dup2 pipes, exec
                                         │
                                         └─▶ JobTable.add(job) ─▶ Feed.on_job_event
                                              │
                                              └─▶ if foreground: waitpid, mark Done
```

## Tokenizer (v1)

A small hand-rolled state machine. Handles:

- Whitespace splits words.
- `|`, `<`, `>`, `>>`, `&`, `;` as operator tokens.
- Single quotes preserve everything literally.
- Double quotes preserve spaces, expand `$VAR` and `${VAR}` from
  `getenv`, honor `\` escapes.
- Backslash outside quotes escapes the next char.

Variable expansion happens at tokenize time; production shells defer
this to evaluation but the educational shell keeps it simple.
Unterminated quotes throw `TokenizeError`.

## Parser (v1)

The grammar is informal:

```
input    := pipeline (SEMI pipeline)*
pipeline := command (PIPE command)* BACKGROUND?
command  := WORD WORD* (REDIR WORD)*
REDIR    := < | > | >>
```

Multiple pipelines per line are supported (separated by `;`); each
runs in order. A trailing `&` marks the pipeline as background.

## Runner (v1)

Standard Unix: fork N children, set up N-1 pipes, dup2 stdin/stdout
into the right fds in each child, apply per-command redirects,
exec. The non-obvious bit: every child + the parent both call
`setpgid(child, pgid)` so the order is race-independent — whichever
side wins, the child ends up in the right group.

For tests we expose `make_plan(pipeline)` that captures argvs +
redirects without forking. The `Runner::run` path is exercised in
the CI integration smoke (`echo hi | tr a-z A-Z > out`).

## Job control (v1 + v2)

`JobTable` stores per-job state (`Running` / `Stopped` / `Done`)
and exposes a `JobEventHook`. The hook fires on `add` and on every
`update_state`. The shell wires it to `Feed.on_job_event`, which
buffers SSE-formatted frames:

```
event: job
data: {"id":3,"pgid":12345,"from":"running","to":"done","command":"sleep 1"}
```

`bg`, `fg`, and `kill` (not implemented in this v1; reserved as v4
work) build on top of this table.

## Preemptive scheduler simulator (v3)

The scheduler runs in user-space with millisecond-quantized time:

- A `Task` is a stream of CPU + I/O `Burst`s.
- `simulate_round_robin(tasks, quantum)` runs FIFO with fixed
  quantum.
- `simulate_mlfq(tasks, base_quantum, levels=4)` runs N priority
  queues with quantum doubling per level. Tasks demote on quantum
  exhaustion and promote (back to top) when they return from I/O.

The key insight: I/O-bound interactive tasks naturally sit at the
top level; CPU-bound tasks sink to the bottom after a few quanta.
Once they're demoted, the scheduler doesn't keep preempting between
them, and interactive tasks at the top get long effective slices
without competition. That's where the 40-60% context-switch
reduction on the canonical mixed workload comes from.

### Why this is a faithful model

- Same primitives as Linux's CFS / Windows priority boosting:
  per-task priority, quantum, I/O block + wake.
- Same canonical workload (mixed CPU + interactive) where MLFQ
  outperforms RR in the literature (Solomon, Silberschatz, Tanenbaum
  textbooks).
- Deterministic so CI can gate on the relative result.

### Why it's not a full kernel simulator

- Single CPU. Multi-core requires per-CPU run queues + load
  balancing.
- No priority inversion, no inheritance, no aging-based promotion.
- I/O is a fixed-duration burst, not a model of disk seek + queue.

These are explicit non-goals; production kernel work uses ftrace +
real workloads, not toy simulators.

## What's deliberately not here

- **Job control signals** (Ctrl-Z handling, `bg`/`fg` to resume).
  Reserved for v4: would need `tcsetpgrp` to manage terminal
  ownership on each foreground transition.
- **Glob expansion** (`*.c`). Out of scope; production shells defer
  to libc's `glob()`.
- **Subshell substitution** `$(cmd)`. Recursive parsing not yet
  implemented.
- **Profile / config files**. The shell starts fresh each session.

## Comparison to bash / zsh

This is a teaching-grade shell, not a drop-in replacement. It
covers the patterns you'd verify in an OS class final (fork +
exec + pipes + redirection + job table) plus the scheduler
benchmark on top. Production shells layer line-editing (readline),
completion, scripting, and decades of compatibility shims that
aren't relevant to the demonstration.
