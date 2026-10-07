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

#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
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

// ---------------------------------------------------------------------------
// Build tree (research R2)
// ---------------------------------------------------------------------------

// Decides whether a mapped path (see PathMapper) is analysed at all.
using ScopeFn = std::function<bool(const std::string &mapped)>;

// Header search order of one compile command, as absolute directories.
// Quoted includes search the including file's own directory first.
struct SearchPath {
    std::vector<std::string> quoted;
    std::vector<std::string> angled;
};

struct TranslationUnit {
    std::string source; // mapped path
    std::string object; // absolute path
    std::unordered_set<std::string> deps; // mapped paths of every file it read
    std::size_t searchPath = 0;           // index into BuildTree::searchPaths
};

struct BuildTree {
    std::string generator;
    std::string nm; // CMAKE_NM, or "nm"
    std::vector<TranslationUnit> units;
    std::vector<SearchPath> searchPaths;
    std::string error; // non-empty: environment error, exit 2
};

// Reads every in-scope translation unit of a Unix Makefiles or Ninja tree.
// A missing CMakeCache.txt, an unsupported generator, or an in-scope object
// or depfile that does not exist is reported in `error`, never silently
// skipped: an incomplete tree must not yield fewer edges.
BuildTree loadBuildTree(const PathMapper &mapper, const ScopeFn &inScope);

struct IncludeResolution {
    std::set<std::pair<std::string, std::string>> edges; // (from, to), mapped
    std::set<std::string> seen;                           // in-scope files some TU read
};

// Resolves each in-scope file's direct includes the way its compiler did:
// the first candidate on disk in search order, counted only when that file
// is in the TU's depfile (so inactive #if branches drop out).
IncludeResolution resolveIncludes(const BuildTree &tree, const PathMapper &mapper, const ScopeFn &inScope);

} // namespace archcheck

#endif // ARCHCHECK_INCLUDES_H
