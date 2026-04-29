// Pipeline executor.
//
// Builds N child processes connected by N-1 pipes, applies redirects,
// puts them in a single process group, and waits for foreground jobs.
// Background jobs are recorded in the JobTable and the shell returns
// to the prompt immediately.
//
// The implementation is the standard fork/exec/dup2 pattern; the
// non-obvious parts:
// - Each child puts ITSELF into the new pgid (setpgid before exec)
//   AND the parent does too. We do both because either side can race;
//   doing it in both places makes the order independent.
// - Signals: foreground children inherit default disposition; the
//   shell ignores SIGINT/SIGTSTP while waiting (so Ctrl-C kills the
//   job, not the shell).
//
// For test hermeticity we expose a `Runner` that the test harness
// can stub out (so tests don't actually fork — they verify the plan).

#pragma once
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <string>
#include <vector>

#include "osh/jobs.hpp"
#include "osh/parser.hpp"

namespace osh {

struct ExecPlan {
    // What we'd execute. Captured in tests; the real Runner consumes
    // this and forks.
    std::vector<std::vector<std::string>> argvs;
    std::vector<Redirect> stdin_redir;   // applied to first process
    std::vector<Redirect> stdout_redir;  // applied to last process
    bool background{false};
};

inline ExecPlan make_plan(const Pipeline& p) {
    ExecPlan plan;
    plan.background = p.background;
    for (const auto& c : p.commands) {
        plan.argvs.push_back(c.argv);
    }
    if (!p.commands.empty()) {
        for (const auto& r : p.commands.front().redirects) {
            if (r.kind == Redirect::In) plan.stdin_redir.push_back(r);
        }
        for (const auto& r : p.commands.back().redirects) {
            if (r.kind == Redirect::Out || r.kind == Redirect::Append)
                plan.stdout_redir.push_back(r);
        }
    }
    return plan;
}

// Real executor — only used in non-test paths. Forks N children,
// wires pipes, registers a Job, returns the job id.
class Runner {
   public:
    explicit Runner(JobTable& jobs) : jobs_(jobs) {}

    int run(const Pipeline& p) {
        size_t n = p.commands.size();
        if (n == 0) return -1;

        std::vector<std::array<int, 2>> pipes(n > 0 ? n - 1 : 0);
        for (size_t i = 0; i + 1 < n; ++i) {
            if (pipe(pipes[i].data()) < 0) return -1;
        }

        std::vector<pid_t> pids(n);
        pid_t pgid = 0;

        for (size_t i = 0; i < n; ++i) {
            pid_t pid = fork();
            if (pid < 0) return -1;
            if (pid == 0) {
                // Child.
                setpgid(0, pgid);
                if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO);
                if (i + 1 < n) dup2(pipes[i][1], STDOUT_FILENO);
                for (auto& pp : pipes) { close(pp[0]); close(pp[1]); }
                apply_redirects(p.commands[i].redirects);
                std::vector<char*> argv;
                for (auto& a : p.commands[i].argv) argv.push_back(const_cast<char*>(a.c_str()));
                argv.push_back(nullptr);
                execvp(argv[0], argv.data());
                _exit(127);
            }
            // Parent.
            pids[i] = pid;
            if (i == 0) pgid = pid;
            setpgid(pid, pgid);
        }
        for (auto& pp : pipes) { close(pp[0]); close(pp[1]); }

        Job j;
        j.pgid = pgid;
        j.pids = pids;
        j.background = p.background;
        j.command = render(p);
        int id = jobs_.add(j);

        if (!p.background) {
            int status;
            for (auto pid : pids) waitpid(pid, &status, 0);
            jobs_.update_state(id, JobState::Done,
                               WIFEXITED(status) ? std::optional<int>(WEXITSTATUS(status))
                                                 : std::nullopt);
        }
        return id;
    }

   private:
    JobTable& jobs_;

    static void apply_redirects(const std::vector<Redirect>& rs) {
        for (const auto& r : rs) {
            int fd = -1;
            if (r.kind == Redirect::In) fd = open(r.path.c_str(), O_RDONLY);
            else if (r.kind == Redirect::Out) fd = open(r.path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            else if (r.kind == Redirect::Append) fd = open(r.path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) _exit(1);
            int target = (r.kind == Redirect::In) ? STDIN_FILENO : STDOUT_FILENO;
            dup2(fd, target);
            close(fd);
        }
    }

    static std::string render(const Pipeline& p) {
        std::string s;
        for (size_t i = 0; i < p.commands.size(); ++i) {
            if (i) s += " | ";
            for (size_t j = 0; j < p.commands[i].argv.size(); ++j) {
                if (j) s += ' ';
                s += p.commands[i].argv[j];
            }
        }
        if (p.background) s += " &";
        return s;
    }
};

}  // namespace osh
