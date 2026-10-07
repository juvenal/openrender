/**
 * Project: openRender
 *
 * File: graph.h
 *
 * Description:
 *   Edge classification and domain-cycle detection (data-model.md › Edge,
 *   FR-006). Each edge is allowed by the rule table, allowlisted by an entry
 *   that applies to this platform, or a violation.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ARCHCHECK_GRAPH_H
#define ARCHCHECK_GRAPH_H

#include <string>
#include <vector>

#include "config.h"
#include "report.h"

namespace archcheck {

enum class EdgeStatus { Allowed, Allowlisted, Violation };

struct Edge {
    EdgeKind kind = EdgeKind::Include;
    std::string from;
    std::string to;      // header path, or mangled symbol
    std::string display; // demangled symbol (link only)
    Class fromClass = Class::Infra;
    std::vector<Class> toClasses;
    std::vector<std::string> definedIn; // link only
    EdgeStatus status = EdgeStatus::Allowed;
    std::string registryId; // when allowlisted
};

// Sets each edge's status. `matched[i]` is set for every allowlist entry
// that matched an edge (US2 uses the rest for staleness).
void classifyEdges(std::vector<Edge> &edges, const RuleTable &rules, const std::vector<AllowlistEntry> &allowlist,
                   Platform platform, std::vector<bool> &matched);

// "allowed-dependency table (<From> may depend on: <classes>)"
std::string ruleText(const RuleTable &rules, Class from);

// Every class-level cycle through a non-allowed dependency, each with its
// path (shortest closing path, starting at its alphabetically first class),
// one example edge per hop, and whether any hop holds a violation.
std::vector<Cycle> findCycles(const std::vector<Edge> &edges);

} // namespace archcheck

#endif // ARCHCHECK_GRAPH_H
