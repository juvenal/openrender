/**
 * Project: openRender
 *
 * File: includes.cpp
 *
 * Description:
 *   Depfile parsing and #include extraction (see includes.h).
 *
 *   API skeleton only (spec 022 T007): both functions are stubs so the
 *   parser unit tests are red. The real parsers land with US1 (T024).
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

namespace archcheck {

Parsed<std::vector<DepfileRule>> parseDepfile(std::string_view, std::string_view label) {
    Parsed<std::vector<DepfileRule>> r;
    r.errors.push_back({std::string(label), 0, "not implemented"});
    return r;
}

std::vector<IncludeDirective> extractIncludes(std::string_view) {
    return {};
}

} // namespace archcheck
