// v2: SSE-style streaming layer for job state changes + scheduler
// events. Everything goes through a single Feed so a UI can subscribe
// once.

#pragma once
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "osh/jobs.hpp"

namespace osh {

class Feed {
   public:
    void on_job_event(const Job& before, const Job& after) {
        const char* before_state = state_str(before.state);
        const char* after_state = state_str(after.state);
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "event: job\ndata: {\"id\":%d,\"pgid\":%d,\"from\":\"%s\","
                      "\"to\":\"%s\",\"command\":\"%s\"}\n\n",
                      after.id, (int)after.pgid, before_state, after_state, after.command.c_str());
        std::lock_guard g(mu_);
        frames_.emplace_back(buf);
    }

    void on_sched_event(const std::string& kind, const std::string& payload) {
        std::lock_guard g(mu_);
        char buf[512];
        std::snprintf(buf, sizeof(buf), "event: sched\ndata: {\"kind\":\"%s\",%s}\n\n",
                      kind.c_str(), payload.c_str());
        frames_.emplace_back(buf);
    }

    std::vector<std::string> drain() {
        std::lock_guard g(mu_);
        auto out = std::move(frames_);
        frames_.clear();
        return out;
    }

   private:
    std::mutex mu_;
    std::vector<std::string> frames_;

    static const char* state_str(JobState s) {
        switch (s) {
            case JobState::Running: return "running";
            case JobState::Stopped: return "stopped";
            case JobState::Done: return "done";
        }
        return "?";
    }
};

}  // namespace osh
