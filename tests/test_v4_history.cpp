#include <cstdio>
#include <cstdlib>
#include <string>

#include "check.hpp"
#include "osh/history.hpp"

using namespace osh;

static std::string make_tmp_path() {
    std::string path = "/tmp/osh-history-test.txt";
    std::remove(path.c_str());
    return path;
}

TEST_CASE(empty_history_has_zero_size) {
    History h;
    REQUIRE_EQ(h.size(), (size_t)0);
    REQUIRE(h.empty());
}

TEST_CASE(add_records_lines) {
    History h;
    h.add("ls");
    h.add("pwd");
    REQUIRE_EQ(h.size(), (size_t)2);
}

TEST_CASE(blank_lines_dropped) {
    History h;
    h.add("");
    h.add("ls");
    h.add("");
    REQUIRE_EQ(h.size(), (size_t)1);
}

TEST_CASE(consecutive_dupes_dropped) {
    History h;
    h.add("ls");
    h.add("ls");
    h.add("ls");
    REQUIRE_EQ(h.size(), (size_t)1);
}

TEST_CASE(capacity_drops_oldest) {
    History h(3);
    h.add("a");
    h.add("b");
    h.add("c");
    h.add("d");
    REQUIRE_EQ(h.size(), (size_t)3);
    auto recent = h.recent(3);
    REQUIRE_EQ(recent[0], std::string("b"));
    REQUIRE_EQ(recent[2], std::string("d"));
}

TEST_CASE(grep_returns_newest_first) {
    History h;
    h.add("ls /tmp");
    h.add("cd /home");
    h.add("ls /opt");
    auto matches = h.grep("ls");
    REQUIRE_EQ(matches.size(), (size_t)2);
    REQUIRE_EQ(matches[0], std::string("ls /opt"));  // newest first
    REQUIRE_EQ(matches[1], std::string("ls /tmp"));
}

TEST_CASE(save_then_load_round_trips) {
    auto path = make_tmp_path();
    History h1;
    h1.add("ls");
    h1.add("pwd");
    REQUIRE(h1.save_to_file(path));

    History h2;
    REQUIRE(h2.load_from_file(path));
    REQUIRE_EQ(h2.size(), (size_t)2);
    REQUIRE_EQ(h2.recent(2)[0], std::string("ls"));
    REQUIRE_EQ(h2.recent(2)[1], std::string("pwd"));
    std::remove(path.c_str());
}

TEST_CASE(load_missing_file_returns_false) {
    History h;
    REQUIRE(!h.load_from_file("/tmp/does-not-exist-osh-history"));
}

int main() { return check::run(); }
