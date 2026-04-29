#include "check.hpp"
#include "osh/parser.hpp"

using namespace osh;

TEST_CASE(single_command) {
    auto p = parse(tokenize("ls -la"));
    REQUIRE_EQ(p.size(), (size_t)1);
    REQUIRE_EQ(p[0].commands.size(), (size_t)1);
    REQUIRE_EQ(p[0].commands[0].argv.size(), (size_t)2);
    REQUIRE(!p[0].background);
}

TEST_CASE(pipeline_of_three) {
    auto p = parse(tokenize("a | b | c"));
    REQUIRE_EQ(p.size(), (size_t)1);
    REQUIRE_EQ(p[0].commands.size(), (size_t)3);
    REQUIRE_EQ(p[0].commands[0].argv[0], std::string("a"));
    REQUIRE_EQ(p[0].commands[2].argv[0], std::string("c"));
}

TEST_CASE(redirects_attach_to_correct_command) {
    auto p = parse(tokenize("a < in.txt | b > out.txt"));
    REQUIRE_EQ(p[0].commands.size(), (size_t)2);
    REQUIRE_EQ(p[0].commands[0].redirects.size(), (size_t)1);
    REQUIRE(p[0].commands[0].redirects[0].kind == Redirect::In);
    REQUIRE_EQ(p[0].commands[1].redirects.size(), (size_t)1);
    REQUIRE(p[0].commands[1].redirects[0].kind == Redirect::Out);
}

TEST_CASE(background_marker) {
    auto p = parse(tokenize("sleep 1 &"));
    REQUIRE_EQ(p.size(), (size_t)1);
    REQUIRE(p[0].background);
}

TEST_CASE(multi_pipeline_separated_by_semicolon) {
    auto p = parse(tokenize("a; b; c"));
    REQUIRE_EQ(p.size(), (size_t)3);
}

TEST_CASE(pipe_without_left_command_throws) {
    bool threw = false;
    try { (void)parse(tokenize("| ls")); } catch (const ParseError&) { threw = true; }
    REQUIRE(threw);
}

TEST_CASE(redirect_without_path_throws) {
    bool threw = false;
    try { (void)parse(tokenize("ls >")); } catch (const ParseError&) { threw = true; }
    REQUIRE(threw);
}

int main() { return check::run(); }
