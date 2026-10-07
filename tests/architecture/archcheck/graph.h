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

#include <map>
#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "report.h"

namespace archcheck {

    enum class EdgeStatus { Allowed,
                            Allowlisted,
                            Violation };

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
    void classifyEdges(std::vector<Edge> &edges, const RuleTable &rules, const std::vector<AllowlistEntry> &allowlist, Platform platform, std::vector<bool> &matched);

    struct AllowlistCheck {
            std::vector<EntryIssue> stale;    // applying, compiled, edge gone or now allowed
            std::vector<EntryIssue> badIds;   // registry id unknown or Resolved
            std::vector<EntryIssue> notBuilt; // applying, referencing file not compiled
    };

    // Validates every allowlist entry that applies to `platform` (FR-009,
    // FR-010, FR-010a). A file counts as compiled for an include entry when some
    // translation unit read it (`includeCompiled`), and for a link entry when it
    // was compiled into an object (`linkCompiled`).
    AllowlistCheck checkAllowlist(const std::vector<AllowlistEntry> &allowlist, const std::vector<bool> &matched, const std::vector<Edge> &edges, const std::map<std::string, RegistryEntry> &registry, Platform platform, const std::set<std::string> &includeCompiled, const std::set<std::string> &linkCompiled);

    // "allowed-dependency table (<From> may depend on: <classes>)"
    std::string ruleText(const RuleTable &rules, Class from);

    // Every class-level cycle through a non-allowed dependency, each with its
    // path (shortest closing path, starting at its alphabetically first class),
    // one example edge per hop, and whether any hop holds a violation.
    std::vector<Cycle> findCycles(const std::vector<Edge> &edges);

} // namespace archcheck

#endif // ARCHCHECK_GRAPH_H
