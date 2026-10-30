/**
 * Project: openRender
 *
 * File: declaration_table.h
 *
 * Description:
 *   Spec 037: reads declarations.txt, the characterization table of the shadeop declaration
 *   parser (one case per line: declaration TAB accepted TAB name TAB prototype, with the escapes
 *   \t \n {empty} {rep:N:text}). Used by ShadeopAbi_ParserReentrant, which runs the same table
 *   the approved ShadeopAbi_DeclarationCharacterization checks, from several threads at once.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OR_TEST_DECLARATION_TABLE_H
#define OR_TEST_DECLARATION_TABLE_H

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

struct SDeclarationCase {
        std::string declaration;
        bool accepted;
        std::string name;
        std::string prototype;
};

namespace declaration_table {

    // Expand \t, \n, {empty} and {rep:N:text}.
    inline std::string expand(const std::string &field) {
        std::string out;
        for (size_t i = 0; i < field.size(); ++i) {
            if (field.compare(i, 7, "{empty}") == 0) {
                i += 6;
            }
            else if (field.compare(i, 5, "{rep:") == 0) {
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

    inline std::vector<std::string> splitTabs(const std::string &line) {
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

    // False if the file cannot be read or a line is malformed.
    inline bool load(const char *path, std::vector<SDeclarationCase> &cases) {
        std::ifstream in(path);
        if (!in)
            return false;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            const std::vector<std::string> fields = splitTabs(line);
            if (fields.size() != 4)
                return false;
            cases.push_back({expand(fields[0]), std::atoi(fields[1].c_str()) != 0, expand(fields[2]), fields[3]});
        }
        return !cases.empty();
    }

} // namespace declaration_table

#endif
