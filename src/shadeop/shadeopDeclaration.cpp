/**
 * Project: openRender
 *
 * File: shadeopDeclaration.cpp
 *
 * Description:
 *   The shadeop declaration parser. See shadeopDeclaration.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "shadeopDeclaration.h"

namespace {

    enum class EToken {
        Type,
        Name,
        Open,
        Close,
        Comma,
        End
    };

    struct SToken {
            EToken kind;
            char letter;      // Type: the prototype letter
            std::string text; // Name
    };

    bool isNameCharacter(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    // The prototype letter of a type word, or 0 when the word is not a type.
    char typeLetter(std::string_view word) {
        if (word == "void")
            return 'o';
        if (word == "float")
            return 'f';
        if (word == "vector")
            return 'v';
        if (word == "normal")
            return 'n';
        if (word == "point")
            return 'p';
        if (word == "color")
            return 'c';
        if (word == "matrix")
            return 'm';
        if (word == "string")
            return 's';
        return 0;
    }

    // The scanner of dso.l: names and type words by longest match, `output` skipped, punctuation, and every
    // other character skipped.
    class CScanner {
        public:
            explicit CScanner(std::string_view source) : text(source), at(0) {}

            SToken next() {
                for (;;) {
                    if (at >= text.size())
                        return {EToken::End, 0, std::string()};
                    const char c = text[at];
                    if (c == '(') {
                        ++at;
                        return {EToken::Open, 0, std::string()};
                    }
                    if (c == ')') {
                        ++at;
                        return {EToken::Close, 0, std::string()};
                    }
                    if (c == ',') {
                        ++at;
                        return {EToken::Comma, 0, std::string()};
                    }
                    if (!isNameCharacter(c)) {
                        ++at; // blanks, tabs, newlines and anything else
                        continue;
                    }
                    size_t end = at;
                    while (end < text.size() && isNameCharacter(text[end]))
                        ++end;
                    const std::string_view word = text.substr(at, end - at);
                    at = end;
                    if (word == "output")
                        continue;
                    if (const char letter = typeLetter(word))
                        return {EToken::Type, letter, std::string()};
                    return {EToken::Name, 0, std::string(word)};
                }
            }

        private:
            std::string_view text;
            size_t at;
    };

} // namespace

bool parseShadeopDeclaration(std::string_view decl, SShadeopDeclaration &out) {
    CScanner scanner(decl);
    SToken token = scanner.next();

    // return type
    if (token.kind != EToken::Type)
        return false;
    std::string prototype(1, token.letter);
    prototype += '=';

    // name
    token = scanner.next();
    if (token.kind != EToken::Name)
        return false;
    std::string name = token.text;

    // '('
    token = scanner.next();
    if (token.kind != EToken::Open)
        return false;

    // [ parameter { ',' parameter } ] ')'
    token = scanner.next();
    if (token.kind != EToken::Close) {
        for (;;) {
            if (token.kind != EToken::Type)
                return false;
            prototype += token.letter;
            token = scanner.next();
            if (token.kind == EToken::Name) // the parameter's name is not used
                token = scanner.next();
            if (token.kind == EToken::Comma) {
                token = scanner.next();
                continue;
            }
            if (token.kind == EToken::Close)
                break;
            return false;
        }
    }

    // nothing may follow
    if (scanner.next().kind != EToken::End)
        return false;

    out.name = std::move(name);
    out.prototype = std::move(prototype);
    return true;
}
