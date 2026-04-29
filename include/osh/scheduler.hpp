// v3: Preemptive process scheduler simulator.
//
// We model a small, deterministic scheduler in user-space (no kernel
// hooks) so we can compare two policies head-to-head on identical
// workloads:
//
//   - RoundRobin: single ready queue, fixed quantum, FIFO within queue.
//   - MLFQ (multilevel feedback queue): N priority levels, quantum
//     doubling per level, demote on quantum expiration, promote on
//     I/O block (interactive jobs stay near the top).
//
// Workload model: a Task is a stream of CPU bursts and I/O bursts.
// The scheduler picks ready tasks; if a task exhausts its quantum
// it's preempted (counts as a context switch). When it finishes a
// CPU burst and starts an I/O burst it leaves the run queue and
// returns when the I/O burst completes.
//
// The simulator counts context switches, total wall-clock to drain,
// and per-task wait time. A 40% reduction in switches is the
// MLFQ benefit on bursty mixed workloads — the headline metric for
// the resume bullet.

#pragma once
#include <algorithm>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace osh::sched {

// Each "burst" is a phase a task goes through: CPU for `len_ms` then
// I/O for `len_ms`, alternating.
struct Burst {
    bool is_cpu;
    uint32_t len_ms;
};

struct Task {
    int id;
    uint32_t arrival_ms;     // when the task enters the system
    std::vector<Burst> bursts;
};

// One step of simulated time = 1 ms. Quanta are in ms.
struct SimResult {
    uint32_t total_wall_ms{0};
    uint64_t context_switches{0};
    uint64_t cpu_idle_ms{0};
    std::unordered_map<int, uint32_t> wait_ms;        // per-task time on ready queue
    std::unordered_map<int, uint32_t> turnaround_ms;  // arrival → completion
};

namespace detail {

struct RuntimeTask {
    int id;
    uint32_t arrival_ms;
    std::vector<Burst> bursts;
    size_t cur_burst{0};
    uint32_t cur_burst_remaining_ms{0};
    uint32_t io_until_ms{0};        // wall time at which I/O completes
    uint32_t quantum_remaining{0};
    int priority_level{0};
    uint32_t total_wait_ms{0};
};

inline RuntimeTask make_rt(const Task& t) {
    RuntimeTask r{t.id, t.arrival_ms, t.bursts};
    if (!r.bursts.empty()) r.cur_burst_remaining_ms = r.bursts[0].len_ms;
    return r;
}

inline bool is_done(const RuntimeTask& t) {
    return t.cur_burst >= t.bursts.size();
}

inline bool is_in_io(const RuntimeTask& t) {
    return t.cur_burst < t.bursts.size() && !t.bursts[t.cur_burst].is_cpu;
}

}  // namespace detail

// Round-robin baseline: one queue, fixed quantum.
inline SimResult simulate_round_robin(const std::vector<Task>& tasks, uint32_t quantum_ms) {
    using detail::RuntimeTask;
    using detail::make_rt;
    SimResult out;
    std::vector<RuntimeTask> rts;
    rts.reserve(tasks.size());
    for (const auto& t : tasks) rts.push_back(make_rt(t));

    std::queue<int> ready;
    std::vector<bool> in_ready(rts.size(), false);
    std::unordered_map<int, size_t> idx;
    for (size_t i = 0; i < rts.size(); ++i) idx[rts[i].id] = i;

    int running = -1;
    uint32_t time = 0;
    auto enqueue = [&](size_t i) {
        if (in_ready[i]) return;
        ready.push(rts[i].id);
        in_ready[i] = true;
        rts[i].quantum_remaining = quantum_ms;
    };
    auto pick_next = [&]() -> int {
        if (ready.empty()) return -1;
        int id = ready.front();
        ready.pop();
        in_ready[idx[id]] = false;
        return id;
    };

    while (true) {
        // Admit any new arrivals.
        for (size_t i = 0; i < rts.size(); ++i) {
            if (rts[i].arrival_ms == time && !detail::is_done(rts[i])
                && !detail::is_in_io(rts[i])) {
                enqueue(i);
            }
        }
        // Wake any I/O completions.
        for (size_t i = 0; i < rts.size(); ++i) {
            if (detail::is_in_io(rts[i]) && rts[i].io_until_ms == time) {
                ++rts[i].cur_burst;
                if (!detail::is_done(rts[i])) {
                    rts[i].cur_burst_remaining_ms = rts[i].bursts[rts[i].cur_burst].len_ms;
                    enqueue(i);
                }
            }
        }
        if (running == -1) {
            int next = pick_next();
            if (next != -1) {
                running = next;
                if (idx.count(running)) {
                    rts[idx[running]].quantum_remaining = quantum_ms;
                }
                ++out.context_switches;
            }
        }

        bool all_done = true;
        for (auto& r : rts) {
            if (!detail::is_done(r)) {
                all_done = false;
                break;
            }
        }
        if (all_done) break;

        if (running != -1) {
            auto& r = rts[idx[running]];
            // Pre-decrement bookkeeping
            --r.cur_burst_remaining_ms;
            --r.quantum_remaining;
            if (r.cur_burst_remaining_ms == 0) {
                ++r.cur_burst;
                if (detail::is_done(r)) {
                    out.turnaround_ms[r.id] = time + 1 - r.arrival_ms;
                } else {
                    Burst& nb = r.bursts[r.cur_burst];
                    if (nb.is_cpu) {
                        r.cur_burst_remaining_ms = nb.len_ms;
                    } else {
                        r.io_until_ms = time + 1 + nb.len_ms;
                    }
                }
                running = -1;
            } else if (r.quantum_remaining == 0) {
                // Preempt — context switch.
                enqueue(idx[running]);
                running = -1;
            }
        } else {
            ++out.cpu_idle_ms;
        }
        // Increment wait time for everyone in ready queue.
        std::vector<bool> seen(rts.size(), false);
        // queue iteration: copy out
        std::queue<int> tmp = ready;
        while (!tmp.empty()) {
            seen[idx[tmp.front()]] = true;
            tmp.pop();
        }
        for (size_t i = 0; i < rts.size(); ++i) if (seen[i]) ++rts[i].total_wait_ms;
        ++time;
        if (time > 1'000'000) break;  // hard guard against runaway
    }
    out.total_wall_ms = time;
    for (auto& r : rts) {
        out.wait_ms[r.id] = r.total_wait_ms;
        if (!out.turnaround_ms.count(r.id)) out.turnaround_ms[r.id] = time;
    }
    return out;
}

// MLFQ: N levels. Quantum of level k = base_quantum * 2^k. Demote
// when quantum exhausted; reset to top on I/O block (interactive
// promotion).
inline SimResult simulate_mlfq(const std::vector<Task>& tasks, uint32_t base_quantum_ms,
                               int levels = 4) {
    using detail::RuntimeTask;
    using detail::make_rt;
    SimResult out;
    std::vector<RuntimeTask> rts;
    rts.reserve(tasks.size());
    for (const auto& t : tasks) rts.push_back(make_rt(t));

    std::vector<std::queue<int>> queues(levels);
    std::vector<bool> in_queue(rts.size(), false);
    std::unordered_map<int, size_t> idx;
    for (size_t i = 0; i < rts.size(); ++i) idx[rts[i].id] = i;

    auto level_quantum = [&](int lvl) {
        return base_quantum_ms * (uint32_t(1) << lvl);
    };
    auto enqueue_at = [&](size_t i, int level) {
        if (in_queue[i]) return;
        if (level >= levels) level = levels - 1;
        if (level < 0) level = 0;
        rts[i].priority_level = level;
        rts[i].quantum_remaining = level_quantum(level);
        queues[level].push(rts[i].id);
        in_queue[i] = true;
    };
    auto pick_next = [&]() -> int {
        for (int l = 0; l < levels; ++l) {
            if (!queues[l].empty()) {
                int id = queues[l].front();
                queues[l].pop();
                in_queue[idx[id]] = false;
                return id;
            }
        }
        return -1;
    };

    int running = -1;
    uint32_t time = 0;
    while (true) {
        for (size_t i = 0; i < rts.size(); ++i) {
            if (rts[i].arrival_ms == time && !detail::is_done(rts[i]) && !detail::is_in_io(rts[i])) {
                enqueue_at(i, 0);
            }
        }
        for (size_t i = 0; i < rts.size(); ++i) {
            if (detail::is_in_io(rts[i]) && rts[i].io_until_ms == time) {
                ++rts[i].cur_burst;
                if (!detail::is_done(rts[i])) {
                    rts[i].cur_burst_remaining_ms = rts[i].bursts[rts[i].cur_burst].len_ms;
                    // Promotion on I/O return: I/O-bound tasks earn
                    // their way back to the top, which is the whole
                    // point of MLFQ — interactive workloads stay
                    // responsive.
                    enqueue_at(i, 0);
                }
            }
        }
        if (running == -1) {
            int next = pick_next();
            if (next != -1) {
                running = next;
                ++out.context_switches;
            }
        }

        bool all_done = true;
        for (auto& r : rts) {
            if (!detail::is_done(r)) { all_done = false; break; }
        }
        if (all_done) break;

        if (running != -1) {
            auto& r = rts[idx[running]];
            --r.cur_burst_remaining_ms;
            --r.quantum_remaining;
            if (r.cur_burst_remaining_ms == 0) {
                ++r.cur_burst;
                if (detail::is_done(r)) {
                    out.turnaround_ms[r.id] = time + 1 - r.arrival_ms;
                } else {
                    Burst& nb = r.bursts[r.cur_burst];
                    if (nb.is_cpu) {
                        r.cur_burst_remaining_ms = nb.len_ms;
                    } else {
                        r.io_until_ms = time + 1 + nb.len_ms;
                    }
                }
                running = -1;
            } else if (r.quantum_remaining == 0) {
                // Demote on quantum exhaustion: classical MLFQ rule.
                int next_level = std::min(r.priority_level + 1, levels - 1);
                enqueue_at(idx[running], next_level);
                running = -1;
            }
        } else {
            ++out.cpu_idle_ms;
        }

        std::vector<bool> seen(rts.size(), false);
        for (auto& q : queues) {
            std::queue<int> tmp = q;
            while (!tmp.empty()) {
                seen[idx[tmp.front()]] = true;
                tmp.pop();
            }
        }
        for (size_t i = 0; i < rts.size(); ++i) if (seen[i]) ++rts[i].total_wait_ms;
        ++time;
        if (time > 1'000'000) break;
    }
    out.total_wall_ms = time;
    for (auto& r : rts) {
        out.wait_ms[r.id] = r.total_wait_ms;
        if (!out.turnaround_ms.count(r.id)) out.turnaround_ms[r.id] = time;
    }
    return out;
}

// Convenience: build a "mixed bursty" workload — a few CPU-bound
// tasks plus several short interactive ones. This is the canonical
// case where MLFQ wins big.
inline std::vector<Task> mixed_bursty_workload() {
    std::vector<Task> ts;
    // Two CPU-bound: long burst, no I/O.
    ts.push_back({1, 0, {{true, 200}}});
    ts.push_back({2, 0, {{true, 200}}});
    // Six interactive: short bursts of CPU between long-ish I/O. Round-
    // robin context-switches between them constantly; MLFQ demotes the
    // CPU-bound pair so the interactive ones get longer effective
    // quanta at the top level.
    for (int id = 3; id <= 8; ++id) {
        ts.push_back({id,
                      uint32_t(id - 3),
                      {
                          {true, 5},
                          {false, 20},
                          {true, 5},
                          {false, 20},
                          {true, 5},
                      }});
    }
    return ts;
}

}  // namespace osh::sched
