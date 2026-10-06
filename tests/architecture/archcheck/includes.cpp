/**
 * Project: openRender
 *
 * File: includes.cpp
 *
 * Description:
 *   Depfile parsing and #include extraction (see includes.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "includes.h"

#include <cctype>

namespace archcheck {

namespace {

bool isSpace(char c) {
    return c == ' ' || c == '\t';
}

// Splits one logical depfile line into words, undoing make's escapes
// ("\ " -> ' ', "\#" -> '#', "$$" -> '$').
std::vector<std::string> depWords(std::string_view s) {
    std::vector<std::string> out;
    std::string cur;
    bool inWord = false;
    for (std::size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        if (c == '\\' && i + 1 < s.size() && (s[i + 1] == ' ' || s[i + 1] == '#')) {
            cur += s[++i];
            inWord = true;
        }
        else if (c == '$' && i + 1 < s.size() && s[i + 1] == '$') {
            cur += '$';
            i++;
            inWord = true;
        }
        else if (isSpace(c)) {
            if (inWord)
                out.push_back(std::move(cur));
            cur.clear();
            inWord = false;
        }
        else {
            cur += c;
            inWord = true;
        }
    }
    if (inWord)
        out.push_back(std::move(cur));
    return out;
}

// Index of the rule separator: the first unescaped ':' followed by
// whitespace or the end of the line.
std::size_t ruleColon(std::string_view s) {
    for (std::size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\') {
            i++;
            continue;
        }
        if (s[i] == ':' && (i + 1 == s.size() || isSpace(s[i + 1])))
            return i;
    }
    return std::string_view::npos;
}

} // namespace

Parsed<std::vector<DepfileRule>> parseDepfile(std::string_view text, std::string_view label) {
    Parsed<std::vector<DepfileRule>> r;
    std::vector<DepfileRule> rules;

    // Join backslash-newline continuations into logical lines, remembering
    // the physical line each one starts on.
    std::string logical;
    int startLine = 0;
    int lineNo = 0;
    auto flush = [&]() {
        std::size_t b = 0;
        while (b < logical.size() && isSpace(logical[b]))
            b++;
        if (b < logical.size()) {
            const std::string_view s(logical);
            const std::size_t colon = ruleColon(s);
            if (colon == std::string_view::npos) {
                r.errors.push_back({std::string(label), startLine, "expected ':' after the rule targets"});
            }
            else {
                DepfileRule rule;
                rule.targets = depWords(s.substr(0, colon));
                rule.deps = depWords(s.substr(colon + 1));
                rules.push_back(std::move(rule));
            }
        }
        logical.clear();
        startLine = 0;
    };

    std::size_t pos = 0;
    while (pos < text.size()) {
        lineNo++;
        const std::size_t nl = text.find('\n', pos);
        std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (startLine == 0)
            startLine = lineNo;
        // An odd number of trailing backslashes continues the line.
        std::size_t slashes = 0;
        while (slashes < line.size() && line[line.size() - 1 - slashes] == '\\')
            slashes++;
        if (slashes % 2 == 1) {
            logical.append(line.substr(0, line.size() - 1));
            logical += ' ';
            continue;
        }
        logical.append(line);
        flush();
    }
    flush();

    if (r.errors.empty())
        r.value = std::move(rules);
    return r;
}

std::vector<IncludeDirective> extractIncludes(std::string_view source) {
    std::vector<IncludeDirective> out;
    bool inBlockComment = false;
    int lineNo = 0;
    std::size_t pos = 0;

    while (pos < source.size()) {
        lineNo++;
        const std::size_t nl = source.find('\n', pos);
        const std::string_view line =
            source.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = nl == std::string_view::npos ? source.size() : nl + 1;

        // The line with comments blanked out. String and character literals
        // are copied through so comment markers inside them are not taken
        // for comments.
        std::string code;
        code.reserve(line.size());
        for (std::size_t i = 0; i < line.size(); i++) {
            const char c = line[i];
            if (inBlockComment) {
                if (c == '*' && i + 1 < line.size() && line[i + 1] == '/') {
                    inBlockComment = false;
                    i++;
                }
                code += ' ';
            }
            else if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
                break;
            }
            else if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
                inBlockComment = true;
                code += ' ';
                i++;
            }
            else if (c == '"' || c == '\'') {
                code += c;
                for (i++; i < line.size() && line[i] != c; i++) {
                    if (line[i] == '\\' && i + 1 < line.size())
                        code += line[i++];
                    code += line[i];
                }
                if (i < line.size())
                    code += c;
            }
            else {
                code += c;
            }
        }

        std::size_t i = 0;
        auto skipSpace = [&]() {
            while (i < code.size() && std::isspace(static_cast<unsigned char>(code[i])))
                i++;
        };
        skipSpace();
        if (i >= code.size() || code[i] != '#')
            continue;
        i++;
        skipSpace();
        const std::size_t kw = i;
        while (i < code.size() && (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_'))
            i++;
        if (std::string_view(code).substr(kw, i - kw) != "include")
            continue;
        skipSpace();
        if (i >= code.size() || (code[i] != '"' && code[i] != '<'))
            continue; // computed include (#include MACRO)
        const char close = code[i] == '"' ? '"' : '>';
        const std::size_t end = code.find(close, i + 1);
        if (end == std::string::npos)
            continue;
        out.push_back({code.substr(i + 1, end - i - 1), close == '>', lineNo});
    }
    return out;
}

} // namespace archcheck
