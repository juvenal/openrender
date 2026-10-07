/**
 * Project: openRender
 *
 * File: symbols.h
 *
 * Description:
 *   Link-level edge inputs (research R3): batched `nm -g -A` over the
 *   in-scope objects, symbol tables keyed by source file, and attribution of
 *   undefined references to the classes that define them (FR-004).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ARCHCHECK_SYMBOLS_H
#define ARCHCHECK_SYMBOLS_H

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "config.h"

namespace archcheck {

struct SymbolTable {
    // Mangled symbol (macOS leading '_' removed) -> defining source files.
    std::map<std::string, std::set<std::string>> definers;
    // Source file -> symbols it references but does not define.
    std::map<std::string, std::set<std::string>> references;
    std::string error; // non-empty: environment error, exit 2
};

// Runs `<nm> -g -A` over the objects in batches. Each object is attributed
// to its source file, so a source compiled into several object libraries is
// one entry. stripUnderscore removes the Mach-O leading '_'.
SymbolTable scanSymbols(const std::string &nm, const std::vector<std::pair<std::string, std::string>> &objectSources,
                        bool stripUnderscore);

struct LinkEdge {
    std::string from;   // referencing source file
    std::string symbol; // mangled
    std::vector<std::string> definedIn;
    std::vector<Class> toClasses;
};

using ClassOf = std::function<std::optional<Class>(const std::string &mapped)>;

// One edge per (source, symbol), ignoring a reference that nothing in the
// project defines, or that any definer in the referencing class or in Infra
// satisfies; otherwise attributed to every remaining definer class.
std::vector<LinkEdge> attributeLinkEdges(const SymbolTable &table, const ClassOf &classOf);

// Demangled form for display; the input itself when it is not mangled.
std::string demangle(const std::string &symbol);

} // namespace archcheck

#endif // ARCHCHECK_SYMBOLS_H
