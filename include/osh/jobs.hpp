// Job control. A Job is a pipeline of one or more processes that
// share a process group; we track them so `jobs`, `fg`, and `bg` work.
//
// State transitions:
//   Running ─Stop─▶ Stopped ─Cont─▶ Running
//        └─Exit/Signal─▶ Done
//
// We expose state via a JobTable + a streaming hook (v2). The
// interpreter pushes events as job state changes.

#pragma once
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/types.h>

namespace osh {

enum class JobState : uint8_t { Running, Stopped, Done };

struct Job {
    int id{0};
    pid_t pgid{0};
    std::vector<pid_t> pids;
    std::string command;  // human-readable representation
    JobState state{JobState::Running};
    bool background{true};
    std::optional<int> exit_status;  // populated when state == Done
    std::chrono::system_clock::time_point started;
};

using JobEventHook = std::function<void(const Job& before, const Job& after)>;

class JobTable {
   public:
    int add(Job j) {
        std::lock_guard g(mu_);
        j.id = ++next_id_;
        j.started = std::chrono::system_clock::now();
        Job before = j;
        before.state = JobState::Done;  // placeholder
        jobs_[j.id] = j;
        if (hook_) hook_(before, j);
        return j.id;
    }

    void update_state(int job_id, JobState s, std::optional<int> exit_status = std::nullopt) {
        std::lock_guard g(mu_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) return;
        Job before = it->second;
        it->second.state = s;
        if (exit_status) it->second.exit_status = exit_status;
        if (hook_) hook_(before, it->second);
    }

    std::optional<Job> get(int job_id) const {
        std::lock_guard g(mu_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<Job> all() const {
        std::lock_guard g(mu_);
        std::vector<Job> out;
        out.reserve(jobs_.size());
        for (const auto& [_, j] : jobs_) out.push_back(j);
        return out;
    }

    void on_event(JobEventHook h) {
        std::lock_guard g(mu_);
        hook_ = std::move(h);
    }

    void clear_done() {
        std::lock_guard g(mu_);
        for (auto it = jobs_.begin(); it != jobs_.end();) {
            if (it->second.state == JobState::Done) it = jobs_.erase(it);
            else ++it;
        }
    }

   private:
    mutable std::mutex mu_;
    std::unordered_map<int, Job> jobs_;
    int next_id_{0};
    JobEventHook hook_;
};

}  // namespace osh
