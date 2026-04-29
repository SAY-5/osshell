// Interactive shell entry point. Reads lines, tokenizes, parses,
// dispatches built-ins or executes pipelines. Background jobs land
// in the JobTable; foreground jobs block until they complete.

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "osh/executor.hpp"
#include "osh/jobs.hpp"
#include "osh/parser.hpp"
#include "osh/stream.hpp"
#include "osh/tokenizer.hpp"

using namespace osh;

static int builtin(const std::vector<std::string>& argv, JobTable& jobs) {
    if (argv.empty()) return -1;
    const auto& cmd = argv[0];
    if (cmd == "exit") std::exit(argv.size() > 1 ? std::atoi(argv[1].c_str()) : 0);
    if (cmd == "cd") {
        const char* home = std::getenv("HOME");
        const char* target = argv.size() > 1 ? argv[1].c_str() : (home ? home : "/");
        if (chdir(target) < 0) std::perror("cd");
        return 0;
    }
    if (cmd == "pwd") {
        char buf[1024];
        if (getcwd(buf, sizeof(buf))) std::puts(buf);
        return 0;
    }
    if (cmd == "jobs") {
        for (const auto& j : jobs.all()) {
            const char* s = j.state == JobState::Running ? "running"
                          : j.state == JobState::Stopped ? "stopped"
                                                          : "done";
            std::printf("[%d] %s  %s\n", j.id, s, j.command.c_str());
        }
        return 0;
    }
    return -1;
}

int main() {
    JobTable jobs;
    Feed feed;
    jobs.on_event([&](const Job& b, const Job& a) { feed.on_job_event(b, a); });
    Runner runner(jobs);

    std::string line;
    while (std::cout << "osh> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            auto toks = tokenize(line);
            auto pipelines = parse(toks);
            for (auto& p : pipelines) {
                if (!p.commands.empty() && p.commands.size() == 1
                    && builtin(p.commands[0].argv, jobs) == 0) {
                    continue;
                }
                runner.run(p);
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "osh: %s\n", e.what());
        }
        // Drain SSE frames to stderr for the dashboard. In production
        // we'd push to a Unix socket; stderr keeps the smoke binary
        // self-contained.
        for (const auto& f : feed.drain()) std::fputs(f.c_str(), stderr);
    }
    return 0;
}
