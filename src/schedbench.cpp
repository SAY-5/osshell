// Scheduler benchmark binary. Runs the canonical mixed-bursty
// workload through both round-robin and MLFQ, prints a comparison
// report. The headline metric for the resume bullet is the context-
// switch reduction.

#include <cstdio>
#include <cstdlib>

#include "osh/scheduler.hpp"

int main(int argc, char** argv) {
    using namespace osh::sched;
    uint32_t base_q = (argc > 1) ? (uint32_t)std::atoi(argv[1]) : 4;
    auto wl = mixed_bursty_workload();

    auto rr = simulate_round_robin(wl, base_q);
    auto mlfq = simulate_mlfq(wl, base_q);

    double red = 0.0;
    if (rr.context_switches > 0) {
        red = 100.0 * (1.0 - double(mlfq.context_switches) / double(rr.context_switches));
    }

    std::printf("workload: mixed-bursty (2 cpu-bound + 6 interactive)\n");
    std::printf("base_quantum_ms: %u\n\n", base_q);
    std::printf("policy        wall_ms  ctx_sw  idle_ms\n");
    std::printf("round_robin   %7u  %6llu  %7llu\n", rr.total_wall_ms,
                (unsigned long long)rr.context_switches, (unsigned long long)rr.cpu_idle_ms);
    std::printf("mlfq          %7u  %6llu  %7llu\n", mlfq.total_wall_ms,
                (unsigned long long)mlfq.context_switches, (unsigned long long)mlfq.cpu_idle_ms);
    std::printf("\ncontext-switch reduction: %.1f%% (lower is better)\n", red);
    return red >= 30.0 ? 0 : 1;  /* CI gate: must be >= 30% */
}
