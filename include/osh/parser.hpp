// Parser turns tokens into a Pipeline.
//
// Grammar (informal):
//   pipeline    := command (PIPE command)* (BACKGROUND)?
//   command     := WORD WORD* (REDIR WORD)*
//   REDIR       := < | > | >>
//
// Multiple pipelines per line (separated by ;) are supported; the
// interpreter evaluates them in order.

#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "osh/tokenizer.hpp"

namespace osh {

struct Redirect {
    enum Kind { In, Out, Append } kind;
    std::string path;
};

struct Command {
    std::vector<std::string> argv;
    std::vector<Redirect> redirects;
};

struct Pipeline {
    std::vector<Command> commands;
    bool background{false};
};

class ParseError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

inline std::vector<Pipeline> parse(const std::vector<Token>& toks) {
    std::vector<Pipeline> out;
    Pipeline cur;
    Command c;

    auto flush_command = [&] {
        if (!c.argv.empty() || !c.redirects.empty()) {
            cur.commands.push_back(std::move(c));
            c = {};
        }
    };
    auto flush_pipeline = [&] {
        flush_command();
        if (!cur.commands.empty()) {
            out.push_back(std::move(cur));
            cur = {};
        }
    };

    for (size_t i = 0; i < toks.size(); ++i) {
        const auto& t = toks[i];
        switch (t.kind) {
            case TokKind::Word:
                c.argv.push_back(t.text);
                break;
            case TokKind::Pipe:
                flush_command();
                if (cur.commands.empty()) throw ParseError("pipe without left command");
                break;
            case TokKind::Semicolon:
                flush_pipeline();
                break;
            case TokKind::Background:
                cur.background = true;
                flush_pipeline();
                break;
            case TokKind::LessThan:
            case TokKind::GreaterThan:
            case TokKind::Append: {
                if (i + 1 >= toks.size() || toks[i + 1].kind != TokKind::Word)
                    throw ParseError("redirection without target path");
                Redirect r;
                r.kind = (t.kind == TokKind::LessThan) ? Redirect::In
                       : (t.kind == TokKind::Append)   ? Redirect::Append
                                                       : Redirect::Out;
                r.path = toks[i + 1].text;
                c.redirects.push_back(r);
                ++i;
                break;
            }
        }
    }
    flush_pipeline();
    return out;
}

}  // namespace osh
