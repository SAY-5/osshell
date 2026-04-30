// v4: persistent command history with grep + truncation.
//
// The interactive shell from v1 forgets every command at exit.
// v4 adds a History buffer that:
//
//  - Records each line submitted (filtered: blank lines + duplicates
//    of the previous command are skipped, matching bash's HISTCONTROL
//    convention).
//  - Persists to ~/.osh_history on save_to_file().
//  - Replays from the file on load_from_file() at startup.
//  - Supports grep(pattern) for fzf-style fuzzy lookup integration.
//
// Implementation is a deque so the recent-N is O(1) on both ends
// without thrashing memory. The file format is one line per
// command — same as bash, so users can `cat` or share the file.

#pragma once
#include <algorithm>
#include <cstdio>
#include <deque>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace osh {

class History {
   public:
    explicit History(size_t capacity = 1000) : capacity_(capacity) {}

    void add(std::string_view line) {
        if (line.empty()) return;
        std::string s(line);
        if (!entries_.empty() && entries_.back() == s) return;  // skip dup
        entries_.push_back(std::move(s));
        while (entries_.size() > capacity_) entries_.pop_front();
    }

    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    // Last `n` commands, newest last.
    std::vector<std::string> recent(size_t n) const {
        std::vector<std::string> out;
        size_t take = std::min(n, entries_.size());
        for (size_t i = entries_.size() - take; i < entries_.size(); ++i) {
            out.push_back(entries_[i]);
        }
        return out;
    }

    // Substring match in newest-first order.
    std::vector<std::string> grep(std::string_view pattern) const {
        std::vector<std::string> out;
        for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
            if (it->find(pattern) != std::string::npos) out.push_back(*it);
        }
        return out;
    }

    bool save_to_file(const std::string& path) const {
        std::ofstream f(path, std::ios::trunc);
        if (!f) return false;
        for (const auto& e : entries_) f << e << '\n';
        return f.good();
    }

    bool load_from_file(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string line;
        entries_.clear();
        while (std::getline(f, line)) {
            if (!line.empty()) entries_.push_back(line);
        }
        while (entries_.size() > capacity_) entries_.pop_front();
        return true;
    }

   private:
    std::deque<std::string> entries_;
    size_t capacity_;
};

}  // namespace osh
