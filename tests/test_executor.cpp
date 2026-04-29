// Executor tests verify the *plan* extracted from parsed pipelines.
// We don't fork in tests — that would require a real shell environment
// and make CI flaky. The Runner's correctness is exercised indirectly
// via integration smoke in CI.

#include "check.hpp"
#include "osh/executor.hpp"

using namespace osh;

TEST_CASE(plan_captures_argvs) {
    auto p = parse(tokenize("ls -la | grep foo"));
    auto plan = make_plan(p[0]);
    REQUIRE_EQ(plan.argvs.size(), (size_t)2);
    REQUIRE_EQ(plan.argvs[0][0], std::string("ls"));
    REQUIRE_EQ(plan.argvs[1][0], std::string("grep"));
}

TEST_CASE(plan_separates_in_and_out_redirects) {
    auto p = parse(tokenize("a < in | b > out"));
    auto plan = make_plan(p[0]);
    REQUIRE_EQ(plan.stdin_redir.size(), (size_t)1);
    REQUIRE_EQ(plan.stdout_redir.size(), (size_t)1);
    REQUIRE_EQ(plan.stdin_redir[0].path, std::string("in"));
    REQUIRE_EQ(plan.stdout_redir[0].path, std::string("out"));
}

TEST_CASE(plan_carries_background_flag) {
    auto p = parse(tokenize("sleep 5 &"));
    auto plan = make_plan(p[0]);
    REQUIRE(plan.background);
}

int main() { return check::run(); }
