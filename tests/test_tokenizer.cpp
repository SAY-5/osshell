#include <cstdlib>

#include "check.hpp"
#include "osh/tokenizer.hpp"

using namespace osh;

TEST_CASE(simple_words) {
    auto t = tokenize("ls -la /tmp");
    REQUIRE_EQ(t.size(), (size_t)3);
    REQUIRE_EQ(t[0].text, std::string("ls"));
    REQUIRE_EQ(t[1].text, std::string("-la"));
    REQUIRE_EQ(t[2].text, std::string("/tmp"));
}

TEST_CASE(operators_separate) {
    auto t = tokenize("a | b > out");
    REQUIRE_EQ(t.size(), (size_t)5);
    REQUIRE(t[1].kind == TokKind::Pipe);
    REQUIRE(t[3].kind == TokKind::GreaterThan);
}

TEST_CASE(append_operator_recognized) {
    auto t = tokenize("a >> log");
    REQUIRE_EQ(t.size(), (size_t)3);
    REQUIRE(t[1].kind == TokKind::Append);
}

TEST_CASE(double_quotes_keep_spaces) {
    auto t = tokenize("echo \"hello world\"");
    REQUIRE_EQ(t.size(), (size_t)2);
    REQUIRE_EQ(t[1].text, std::string("hello world"));
}

TEST_CASE(single_quotes_skip_expansion) {
    setenv("FOO", "bar", 1);
    auto t = tokenize("echo '$FOO'");
    REQUIRE_EQ(t.size(), (size_t)2);
    REQUIRE_EQ(t[1].text, std::string("$FOO"));
}

TEST_CASE(double_quotes_expand_vars) {
    setenv("FOO", "bar", 1);
    auto t = tokenize("echo \"$FOO\"");
    REQUIRE_EQ(t.size(), (size_t)2);
    REQUIRE_EQ(t[1].text, std::string("bar"));
}

TEST_CASE(backslash_escapes_next_char) {
    auto t = tokenize("echo a\\ b");
    REQUIRE_EQ(t.size(), (size_t)2);
    REQUIRE_EQ(t[1].text, std::string("a b"));
}

TEST_CASE(unterminated_quote_throws) {
    bool threw = false;
    try { (void)tokenize("echo \"oops"); } catch (const TokenizeError&) { threw = true; }
    REQUIRE(threw);
}

int main() { return check::run(); }
