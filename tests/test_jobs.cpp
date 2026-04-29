#include "check.hpp"
#include "osh/jobs.hpp"

using namespace osh;

TEST_CASE(add_assigns_unique_ids) {
    JobTable jt;
    Job a, b;
    a.command = "x";
    b.command = "y";
    int ida = jt.add(a);
    int idb = jt.add(b);
    REQUIRE(ida != idb);
}

TEST_CASE(update_state_changes_record) {
    JobTable jt;
    Job j;
    j.command = "x";
    int id = jt.add(j);
    jt.update_state(id, JobState::Stopped);
    auto got = jt.get(id);
    REQUIRE(got.has_value());
    REQUIRE(got->state == JobState::Stopped);
}

TEST_CASE(clear_done_only_removes_finished) {
    JobTable jt;
    Job a, b;
    int ida = jt.add(a);
    int idb = jt.add(b);
    jt.update_state(ida, JobState::Done, 0);
    jt.clear_done();
    REQUIRE(!jt.get(ida).has_value());
    REQUIRE(jt.get(idb).has_value());
}

TEST_CASE(event_hook_fires_on_state_change) {
    JobTable jt;
    int events = 0;
    jt.on_event([&](const Job&, const Job&) { ++events; });
    Job j;
    int id = jt.add(j);
    REQUIRE_EQ(events, 1);
    jt.update_state(id, JobState::Stopped);
    REQUIRE_EQ(events, 2);
}

int main() { return check::run(); }
