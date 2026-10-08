/**
 * Project: openRender
 *
 * File: report.h
 *
 * Description:
 *   archcheck report model and its text / JSON writers. Line shapes and JSON
 *   keys are fixed by specs/022-domain-dependency-guardrails/contracts/
 *   archcheck-cli.md; tests match on the text line prefixes. Output order is
 *   deterministic: by class, then file, then target (SC-007).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ARCHCHECK_REPORT_H
#define ARCHCHECK_REPORT_H

#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "config.h"

namespace archcheck {

    inline constexpr int kExitPass = 0;
    inline constexpr int kExitFail = 1;
    inline constexpr int kExitError = 2;

    struct Violation {
            EdgeKind kind = EdgeKind::Include;
            std::string from;
            std::string to;      // header path, or mangled symbol
            std::string display; // demangled symbol (link only)
            Class fromClass = Class::Infra;
            std::vector<Class> toClasses;
            std::string rule;                   // why it is not allowed
            int line = 0;                       // include: line of the #include
            std::vector<std::string> definedIn; // defining source files (link)
    };

    // An allowlist entry reported back: stale, bad id or not built.
    struct EntryIssue {
            std::string id;
            EdgeKind kind = EdgeKind::Include;
            std::string from;
            std::string to;
            std::string reason; // stale: "no longer present" | "now allowed"; bad id: "unknown" | "resolved"
            int line = 0;
    };

    struct Cycle {
            std::vector<Class> path; // first class repeated at the end
            std::vector<std::string> edges;
            bool failing = false;
    };

    struct ConsumerReach {
            std::string from;
            std::string to;
            Class toClass = Class::Infra;
    };

    struct MetricRow {
            std::string cls; // class name or "TOTAL"
            long refs = 0;
            long distinct = 0;
            std::optional<MetricCounts> baseline;
    };

    struct Report {
            std::string platform;
            long objects = 0;
            long sources = 0;
            long includeEdges = 0;
            long linkEdges = 0;

            std::vector<Violation> violations;
            std::vector<EntryIssue> stale;
            std::vector<EntryIssue> badIds;
            std::vector<Cycle> cycles;
            std::map<std::string, long> allowlisted; // registry id -> edge count
            std::vector<ConsumerReach> consumerReach;
            std::vector<EntryIssue> notBuilt;
            std::vector<std::string> unresolvedHeaders;
            std::vector<MetricRow> metrics;

            long failingCycles() const;
            int exitCode() const; // kExitPass or kExitFail; configuration errors never get this far
    };

    void writeText(const Report &report, FILE *out, bool quiet);
    void writeJson(const Report &report, FILE *out);

} // namespace archcheck

#endif // ARCHCHECK_REPORT_H
