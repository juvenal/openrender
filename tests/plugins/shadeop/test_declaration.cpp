/**
 * Project: openRender
 *
 * File: test_declaration.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), ShadeopAbi_DeclarationCharacterization: the starting
 *   tree's shadeop declaration parser, characterized before it is replaced.
 *
 *   dsoParse(decl, name, prototype) is the flex/bison parser compiled into four libraries
 *   (FR-037). This program calls it over the table in declarations.txt (argv[1]) and checks
 *   the return value, the function name and the prototype of every case. Phase 4 replaces
 *   the parser with parseShadeopDeclaration and re-points this program at it with identical
 *   expectations (only flex's stdout echo of unknown characters differs): the language the
 *   hand-written parser accepts is fixed by this table, not by the new parser's own tests.
 *
 *   The old parser keeps its result in file statics. `name` is a heap string the caller owns,
 *   `prototype` is a strdup of the static buffer; both are freed or copied before the next
 *   call. No case feeds it a prototype of more than 255 letters (its 256-byte buffer is
 *   unbounded).
 *
 *   After the table, one observation is printed and not asserted: a declaration shorter than
 *   the one before it. See the header of declarations.txt.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "ri/parse/dso.h"

namespace {

    // Expand \t, \n, {empty} and {rep:N:text}.
    std::string expand(const std::string &field) {
        std::string out;
        for (size_t i = 0; i < field.size(); ++i) {
            if (field.compare(i, 7, "{empty}") == 0) {
                i += 6;
            }
            else if (field.compare(i, 5, "{rep:") == 0) {
                // {rep:N:text}: N copies of text (text runs to the closing brace).
                const size_t colon = field.find(':', i + 5);
                const size_t close = field.find('}', colon);
                const int n = std::atoi(field.substr(i + 5, colon - (i + 5)).c_str());
                const std::string text = field.substr(colon + 1, close - (colon + 1));
                for (int k = 0; k < n; ++k)
                    out += text;
                i = close;
            }
            else if (field[i] == '\\' && i + 1 < field.size() && field[i + 1] == 't') {
                out += '\t';
                ++i;
            }
            else if (field[i] == '\\' && i + 1 < field.size() && field[i + 1] == 'n') {
                out += '\n';
                ++i;
            }
            else {
                out += field[i];
            }
        }
        return out;
    }

    std::vector<std::string> splitTabs(const std::string &line) {
        std::vector<std::string> fields;
        size_t start = 0;
        for (;;) {
            const size_t tab = line.find('\t', start);
            if (tab == std::string::npos) {
                fields.push_back(line.substr(start));
                return fields;
            }
            fields.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_shadeop_declaration <declarations.txt>\n");
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    int cases = 0, failed = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> fields = splitTabs(line);
        if (fields.size() != 4) {
            fprintf(stderr, "bad line (%zu fields): %s\n", fields.size(), line.c_str());
            return 2;
        }
        const std::string declaration = expand(fields[0]);
        const int expectedReturn = std::atoi(fields[1].c_str());
        const std::string expectedName = expand(fields[2]);
        const std::string expectedPrototype = fields[3];

        char *name = nullptr;
        char *prototype = nullptr;
        const int result = dsoParse(declaration.c_str(), name, prototype);
        ++cases;

        bool ok = (result != 0) == (expectedReturn != 0);
        if (ok && expectedReturn != 0) {
            // Copy before the next call: the parser reuses its static result.
            ok = name != nullptr && prototype != nullptr && expectedName == name && expectedPrototype == prototype;
        }
        if (!ok) {
            ++failed;
            printf("FAIL case %d: [%s]\n  expected: return %d name [%s] prototype [%s]\n  got:      return %d name [%s] prototype [%s]\n", cases,
                   fields[0].size() > 80 ? (fields[0].substr(0, 80) + "...").c_str() : fields[0].c_str(), expectedReturn,
                   expectedName.size() > 40 ? (expectedName.substr(0, 40) + "...").c_str() : expectedName.c_str(), expectedPrototype.c_str(), result,
                   name == nullptr ? "(null)" : (strlen(name) > 40 ? (std::string(name).substr(0, 40) + "...").c_str() : name),
                   prototype == nullptr ? "(null)" : prototype);
        }
        if (result != 0) {
            free(name);
            free(prototype);
        }
    }

    // Observation, not asserted: a shorter declaration after a longer one.
    {
        char *name = nullptr;
        char *prototype = nullptr;
        if (dsoParse("float longer(float, float, float)", name, prototype)) {
            free(name);
            free(prototype);
        }
        if (dsoParse("void shorter()", name, prototype)) {
            printf("observation: \"void shorter()\" after a 3-parameter declaration gives prototype [%s] (a clean parser gives [o=])\n", prototype);
            free(name);
            free(prototype);
        }
    }

    printf("%d case(s), %d failed\n", cases, failed);
    return failed == 0 ? 0 : 1;
}
