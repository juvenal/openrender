/**
 * Project: openRender
 *
 * File: includes.h
 *
 * Description:
 *   Source-level edge inputs (research R2): compiler depfile parsing and
 *   textual extraction of a file's direct #include directives. Directives in
 *   inactive #if branches are still extracted; resolving them against a
 *   translation unit's depfile is what drops the ones never compiled.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ARCHCHECK_INCLUDES_H
#define ARCHCHECK_INCLUDES_H

#include <string>
#include <string_view>
#include <vector>

#include "config.h"

namespace archcheck {

// One make rule of a depfile: `targets...: deps...`.
struct DepfileRule {
    std::vector<std::string> targets;
    std::vector<std::string> deps;
};

// Handles backslash continuations, escaped spaces and multiple targets.
Parsed<std::vector<DepfileRule>> parseDepfile(std::string_view text, std::string_view label);

struct IncludeDirective {
    std::string name; // as written between the quotes or angle brackets
    bool angled = false;
    int line = 0;
};

// Directives inside comments are skipped; computed includes are ignored.
std::vector<IncludeDirective> extractIncludes(std::string_view source);

} // namespace archcheck

#endif // ARCHCHECK_INCLUDES_H
