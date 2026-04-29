#include "check.hpp"
#include "osh/jobs.hpp"
#include "osh/stream.hpp"

using namespace osh;

TEST_CASE(job_event_emits_sse_frame) {
    Feed feed;
    JobTable jt;
    jt.on_event([&](const Job& a, const Job& b) { feed.on_job_event(a, b); });
    Job j;
    j.command = "ls";
    jt.add(j);
    auto frames = feed.drain();
    REQUIRE_EQ(frames.size(), (size_t)1);
    REQUIRE(frames[0].find("event: job\n") == 0);
    REQUIRE(frames[0].find("\"to\":\"running\"") != std::string::npos);
}

TEST_CASE(state_change_emits_second_frame) {
    Feed feed;
    JobTable jt;
    jt.on_event([&](const Job& a, const Job& b) { feed.on_job_event(a, b); });
    Job j;
    j.command = "ls";
    int id = jt.add(j);
    feed.drain();
    jt.update_state(id, JobState::Done, 0);
    auto frames = feed.drain();
    REQUIRE_EQ(frames.size(), (size_t)1);
    REQUIRE(frames[0].find("\"to\":\"done\"") != std::string::npos);
}

TEST_CASE(sched_event_renders_with_payload) {
    Feed feed;
    feed.on_sched_event("preempt", "\"task\":7,\"reason\":\"quantum\"");
    auto frames = feed.drain();
    REQUIRE_EQ(frames.size(), (size_t)1);
    REQUIRE(frames[0].find("event: sched\n") == 0);
    REQUIRE(frames[0].find("\"task\":7") != std::string::npos);
}

TEST_CASE(drain_clears_buffer) {
    Feed feed;
    feed.on_sched_event("x", "");
    REQUIRE(!feed.drain().empty());
    REQUIRE(feed.drain().empty());
}

int main() { return check::run(); }
