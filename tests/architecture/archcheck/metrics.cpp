/**
 * Project: openRender
 *
 * File: metrics.cpp
 *
 * Description:
 *   Renderer-core usage metric (see metrics.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "metrics.h"

#include <cctype>

namespace archcheck {

    namespace {

        bool isIdent(char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

    } // namespace

    std::string stripCommentsAndLiterals(std::string_view s) {
        std::string out(s);
        auto blank = [&](std::size_t from, std::size_t to) {
            for (std::size_t k = from; k < to && k < out.size(); k++)
                if (out[k] != '\n')
                    out[k] = ' ';
        };

        std::size_t i = 0;
        while (i < s.size()) {
            const char c = s[i];
            if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
                const std::size_t end = s.find('\n', i);
                const std::size_t stop = end == std::string_view::npos ? s.size() : end;
                blank(i, stop);
                i = stop;
            }
            else if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
                const std::size_t end = s.find("*/", i + 2);
                const std::size_t stop = end == std::string_view::npos ? s.size() : end + 2;
                blank(i, stop);
                i = stop;
            }
            else if (c == 'R' && i + 1 < s.size() && s[i + 1] == '"' && (i == 0 || !isIdent(s[i - 1]))) {
                // Raw string R"delim( ... )delim"
                const std::size_t open = s.find('(', i + 2);
                if (open == std::string_view::npos) {
                    i++;
                    continue;
                }
                const std::string close = ")" + std::string(s.substr(i + 2, open - i - 2)) + "\"";
                const std::size_t end = s.find(close, open + 1);
                const std::size_t stop = end == std::string_view::npos ? s.size() : end + close.size();
                blank(i + 2, stop - 1);
                i = stop;
            }
            else if ((c == '"' || c == '\'') && i > 0 && s[i - 1] == '\\') {
                // An escaped quote outside any literal (lex/yacc patterns such as
                // \"[^\"]*\") opens nothing.
                i++;
            }
            else if (c == '"' || c == '\'') {
                std::size_t j = i + 1;
                while (j < s.size() && s[j] != c && s[j] != '\n') {
                    if (s[j] == '\\' && j + 1 < s.size())
                        j++;
                    j++;
                }
                blank(i + 1, j);
                i = j + 1;
            }
            else {
                i++;
            }
        }
        return out;
    }

    void countCoreUsage(std::string_view s, CoreUsage &usage) {
        static constexpr std::string_view kClasses[] = {"CRendererContext::", "CRenderer::"};
        for (std::size_t i = 0; i < s.size(); i++) {
            if (s[i] != 'C' || (i > 0 && (isIdent(s[i - 1]) || s[i - 1] == ':')))
                continue;
            for (const std::string_view cls : kClasses) {
                if (s.substr(i, cls.size()) != cls)
                    continue;
                std::size_t j = i + cls.size();
                const std::size_t start = j;
                while (j < s.size() && isIdent(s[j]))
                    j++;
                if (j > start) {
                    usage.refs++;
                    usage.members.insert(std::string(cls) + std::string(s.substr(start, j - start)));
                }
                i = j - 1;
                break;
            }
        }
    }

} // namespace archcheck
