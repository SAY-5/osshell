// Shell tokenizer.
//
// Splits a command line into tokens, recognizing single + double
// quotes, escaped chars, and the special operators | < > >> &.
// Variable expansion ($FOO, ${FOO}) substitutes from getenv at tokenize
// time — production shells defer expansion to evaluation but for an
// educational shell tokenize-time substitution keeps the parser simple.

#pragma once
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace osh {

enum class TokKind : uint8_t {
    Word,
    Pipe,        // |
    LessThan,    // <
    GreaterThan, // >
    Append,      // >>
    Background,  // &
    Semicolon,   // ;
};

struct Token {
    TokKind kind;
    std::string text;
};

class TokenizeError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

inline std::string expand_var(std::string_view name) {
    if (name.empty()) return "";
    std::string n(name);
    const char* v = std::getenv(n.c_str());
    return v ? std::string(v) : std::string{};
}

inline std::vector<Token> tokenize(std::string_view input) {
    std::vector<Token> out;
    std::string cur;
    bool in_word = false;
    auto flush = [&] {
        if (in_word) {
            out.push_back({TokKind::Word, cur});
            cur.clear();
            in_word = false;
        }
    };

    for (size_t i = 0; i < input.size();) {
        char c = input[i];
        if (c == ' ' || c == '\t' || c == '\n') {
            flush();
            ++i;
            continue;
        }
        if (c == '|') { flush(); out.push_back({TokKind::Pipe, "|"}); ++i; continue; }
        if (c == ';') { flush(); out.push_back({TokKind::Semicolon, ";"}); ++i; continue; }
        if (c == '&') { flush(); out.push_back({TokKind::Background, "&"}); ++i; continue; }
        if (c == '<') { flush(); out.push_back({TokKind::LessThan, "<"}); ++i; continue; }
        if (c == '>') {
            flush();
            if (i + 1 < input.size() && input[i + 1] == '>') {
                out.push_back({TokKind::Append, ">>"});
                i += 2;
            } else {
                out.push_back({TokKind::GreaterThan, ">"});
                ++i;
            }
            continue;
        }
        if (c == '"') {
            in_word = true;
            ++i;
            while (i < input.size() && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < input.size()) {
                    cur += input[i + 1];
                    i += 2;
                } else if (input[i] == '$') {
                    ++i;
                    std::string name;
                    if (i < input.size() && input[i] == '{') {
                        ++i;
                        while (i < input.size() && input[i] != '}') name += input[i++];
                        if (i < input.size()) ++i;  // skip }
                    } else {
                        while (i < input.size() && (isalnum((unsigned char)input[i]) || input[i] == '_'))
                            name += input[i++];
                    }
                    cur += expand_var(name);
                } else {
                    cur += input[i++];
                }
            }
            if (i >= input.size()) throw TokenizeError("unterminated double-quote");
            ++i;
            continue;
        }
        if (c == '\'') {
            in_word = true;
            ++i;
            while (i < input.size() && input[i] != '\'') cur += input[i++];
            if (i >= input.size()) throw TokenizeError("unterminated single-quote");
            ++i;
            continue;
        }
        if (c == '$') {
            in_word = true;
            ++i;
            std::string name;
            if (i < input.size() && input[i] == '{') {
                ++i;
                while (i < input.size() && input[i] != '}') name += input[i++];
                if (i < input.size()) ++i;
            } else {
                while (i < input.size() && (isalnum((unsigned char)input[i]) || input[i] == '_'))
                    name += input[i++];
            }
            cur += expand_var(name);
            continue;
        }
        if (c == '\\' && i + 1 < input.size()) {
            in_word = true;
            cur += input[i + 1];
            i += 2;
            continue;
        }
        in_word = true;
        cur += c;
        ++i;
    }
    flush();
    return out;
}

}  // namespace osh
