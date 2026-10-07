/**
 * Project: openRender
 *
 * File: report.cpp
 *
 * Description:
 *   archcheck report writers (see report.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "report.h"

#include <algorithm>
#include <tuple>

namespace archcheck {

namespace {

const char *kindName(EdgeKind k) {
    return k == EdgeKind::Include ? "include" : "link";
}

std::string classList(const std::vector<Class> &classes) {
    std::string s;
    for (const Class c : classes) {
        if (!s.empty())
            s += ",";
        s += className(c);
    }
    return s;
}

std::string cyclePath(const Cycle &c) {
    std::string s;
    for (const Class k : c.path) {
        if (!s.empty())
            s += " -> ";
        s += className(k);
    }
    return s;
}

std::string joined(const std::vector<std::string> &v, const char *sep) {
    std::string s;
    for (const std::string &x : v) {
        if (!s.empty())
            s += sep;
        s += x;
    }
    return s;
}

// Sorted copies, so the writers never depend on discovery order.
std::vector<Violation> sortedViolations(const Report &r) {
    auto v = r.violations;
    std::sort(v.begin(), v.end(), [](const Violation &a, const Violation &b) {
        return std::make_tuple(className(a.fromClass), a.from, a.to, a.kind) <
               std::make_tuple(className(b.fromClass), b.from, b.to, b.kind);
    });
    return v;
}

std::vector<EntryIssue> sortedIssues(std::vector<EntryIssue> v) {
    std::sort(v.begin(), v.end(), [](const EntryIssue &a, const EntryIssue &b) {
        return std::tie(a.id, a.kind, a.from, a.to, a.line) < std::tie(b.id, b.kind, b.from, b.to, b.line);
    });
    return v;
}

std::vector<Cycle> sortedCycles(const Report &r) {
    auto v = r.cycles;
    std::sort(v.begin(), v.end(), [](const Cycle &a, const Cycle &b) { return cyclePath(a) < cyclePath(b); });
    return v;
}

std::vector<ConsumerReach> sortedConsumers(const Report &r) {
    auto v = r.consumerReach;
    std::sort(v.begin(), v.end(), [](const ConsumerReach &a, const ConsumerReach &b) {
        return std::make_tuple(className(a.toClass), a.from, a.to) <
               std::make_tuple(className(b.toClass), b.from, b.to);
    });
    return v;
}

std::vector<MetricRow> sortedMetrics(const Report &r) {
    auto v = r.metrics;
    // Classes alphabetically, TOTAL last.
    std::sort(v.begin(), v.end(), [](const MetricRow &a, const MetricRow &b) {
        return std::make_tuple(a.cls == "TOTAL", a.cls) < std::make_tuple(b.cls == "TOTAL", b.cls);
    });
    return v;
}

void jsonString(FILE *out, std::string_view s) {
    std::fputc('"', out);
    for (const char c : s) {
        switch (c) {
            case '"':
                std::fputs("\\\"", out);
                break;
            case '\\':
                std::fputs("\\\\", out);
                break;
            case '\n':
                std::fputs("\\n", out);
                break;
            case '\t':
                std::fputs("\\t", out);
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    std::fprintf(out, "\\u%04x", static_cast<unsigned>(c));
                else
                    std::fputc(c, out);
        }
    }
    std::fputc('"', out);
}

// Writes `[item, item, ...]`, one item per element via fn.
template <class T, class Fn>
void jsonArray(FILE *out, const std::vector<T> &v, Fn fn) {
    std::fputc('[', out);
    for (std::size_t i = 0; i < v.size(); i++) {
        if (i != 0)
            std::fputs(", ", out);
        fn(v[i]);
    }
    std::fputc(']', out);
}

void jsonIssue(FILE *out, const EntryIssue &e, bool withReason) {
    std::fputs("{\"id\": ", out);
    jsonString(out, e.id);
    if (withReason) {
        std::fputs(", \"reason\": ", out);
        jsonString(out, e.reason);
        std::fprintf(out, ", \"line\": %d", e.line);
    }
    std::fputs(", \"kind\": ", out);
    jsonString(out, kindName(e.kind));
    std::fputs(", \"from\": ", out);
    jsonString(out, e.from);
    std::fputs(", \"to\": ", out);
    jsonString(out, e.to);
    std::fputc('}', out);
}

// Metric delta lines: an increase warns, a decrease suggests lowering.
void metricDelta(FILE *out, const MetricRow &m, const char *what, long now, long base) {
    if (now > base)
        std::fprintf(out, "WARNING   metric %s %s %ld > baseline %ld (+%ld)\n", m.cls.c_str(), what, now,
                     base, now - base);
    else if (now < base)
        std::fprintf(out, "NOTE      metric %s %s %ld < baseline %ld (-%ld); consider lowering the baseline\n",
                     m.cls.c_str(), what, now, base, base - now);
}

} // namespace

long Report::failingCycles() const {
    return std::count_if(cycles.begin(), cycles.end(), [](const Cycle &c) { return c.failing; });
}

int Report::exitCode() const {
    const bool fail = !violations.empty() || !stale.empty() || !badIds.empty() || failingCycles() > 0;
    return fail ? kExitFail : kExitPass;
}

void writeText(const Report &r, FILE *out, bool quiet) {
    if (!quiet)
        std::fprintf(out, "archcheck: platform=%s objects=%ld sources=%ld edges=%ld (include=%ld link=%ld)\n",
                     r.platform.c_str(), r.objects, r.sources, r.includeEdges + r.linkEdges, r.includeEdges,
                     r.linkEdges);

    for (const Violation &v : sortedViolations(r)) {
        const std::string classes = "[" + std::string(className(v.fromClass)) + " -> " + classList(v.toClasses) + "]";
        if (v.kind == EdgeKind::Include)
            std::fprintf(out, "VIOLATION include %s -> %s  %s rule: %s\n", v.from.c_str(), v.to.c_str(),
                         classes.c_str(), v.rule.c_str());
        else
            std::fprintf(out, "VIOLATION link    %s -> %s (%s)  %s defined in: %s\n", v.from.c_str(), v.to.c_str(),
                         v.display.c_str(), classes.c_str(), joined(v.definedIn, ", ").c_str());
    }

    for (const EntryIssue &e : sortedIssues(r.stale))
        std::fprintf(out, "STALE     %s %s %s -> %s  (edge %s; delete the entry)\n", e.id.c_str(), kindName(e.kind),
                     e.from.c_str(), e.to.c_str(), e.reason.c_str());

    for (const EntryIssue &e : sortedIssues(r.badIds)) {
        const std::string why = e.reason == "resolved" ? "registry entry " + e.id + " is Resolved"
                                                       : "no Open registry entry " + e.id;
        std::fprintf(out, "BADID     %s %s %s -> %s  (%s)\n", e.id.c_str(), kindName(e.kind), e.from.c_str(),
                     e.to.c_str(), why.c_str());
    }

    for (const Cycle &c : sortedCycles(r)) {
        if (quiet && !c.failing)
            continue;
        std::fprintf(out, "CYCLE     %s  (via %s)  [%s]\n", cyclePath(c).c_str(), joined(c.edges, "; ").c_str(),
                     c.failing ? "FAILING" : "allowlisted");
    }

    if (!quiet) {
        if (!r.allowlisted.empty()) {
            std::fputs("ALLOWED  ", out);
            for (const auto &[id, n] : r.allowlisted)
                std::fprintf(out, " %s x %ld", id.c_str(), n);
            std::fputc('\n', out);
        }
        for (const ConsumerReach &c : sortedConsumers(r))
            std::fprintf(out, "CONSUMER  %s -> %s  [Consumer -> %s]\n", c.from.c_str(), c.to.c_str(),
                         std::string(className(c.toClass)).c_str());
        for (const EntryIssue &e : sortedIssues(r.notBuilt))
            std::fprintf(out, "NOTBUILT  %s %s %s -> %s  (referencing file not compiled in this build; skipped)\n",
                         e.id.c_str(), kindName(e.kind), e.from.c_str(), e.to.c_str());
        auto unresolved = r.unresolvedHeaders;
        std::sort(unresolved.begin(), unresolved.end());
        for (const std::string &h : unresolved)
            std::fprintf(out, "UNRESOLVED %s  (no compiled translation unit includes it; no edges derived)\n",
                         h.c_str());
    }

    for (const MetricRow &m : sortedMetrics(r)) {
        const MetricCounts base = m.baseline.value_or(MetricCounts{});
        if (!quiet)
            std::fprintf(out, "METRIC    %s refs %ld (baseline %ld)  distinct %ld (baseline %ld)\n", m.cls.c_str(),
                         m.refs, base.refs, m.distinct, base.distinct);
        if (m.refs > base.refs || !quiet)
            metricDelta(out, m, "refs", m.refs, base.refs);
        if (m.distinct > base.distinct || !quiet)
            metricDelta(out, m, "distinct", m.distinct, base.distinct);
    }

    std::fprintf(out, "RESULT    %s violations=%zu stale=%zu badid=%zu failing-cycles=%ld notbuilt=%zu\n",
                 r.exitCode() == kExitPass ? "PASS" : "FAIL", r.violations.size(), r.stale.size(), r.badIds.size(),
                 r.failingCycles(), r.notBuilt.size());
}

void writeJson(const Report &r, FILE *out) {
    std::fputs("{\n  \"platform\": ", out);
    jsonString(out, r.platform);
    std::fprintf(out,
                 ",\n  \"counts\": {\"objects\": %ld, \"sources\": %ld, \"include_edges\": %ld, "
                 "\"link_edges\": %ld}",
                 r.objects, r.sources, r.includeEdges, r.linkEdges);

    std::fputs(",\n  \"violations\": ", out);
    jsonArray(out, sortedViolations(r), [&](const Violation &v) {
        std::fputs("{\"kind\": ", out);
        jsonString(out, kindName(v.kind));
        std::fputs(", \"from\": ", out);
        jsonString(out, v.from);
        std::fputs(", \"to\": ", out);
        jsonString(out, v.to);
        std::fputs(", \"display\": ", out);
        if (v.display.empty())
            std::fputs("null", out);
        else
            jsonString(out, v.display);
        std::fputs(", \"from_class\": ", out);
        jsonString(out, className(v.fromClass));
        std::fputs(", \"to_class\": ", out);
        jsonArray(out, v.toClasses, [&](Class c) { jsonString(out, className(c)); });
        std::fputs(", \"rule\": ", out);
        jsonString(out, v.rule);
        std::fputs(", \"defined_in\": ", out);
        jsonArray(out, v.definedIn, [&](const std::string &s) { jsonString(out, s); });
        std::fputc('}', out);
    });

    std::fputs(",\n  \"stale\": ", out);
    jsonArray(out, sortedIssues(r.stale), [&](const EntryIssue &e) { jsonIssue(out, e, false); });

    std::fputs(",\n  \"bad_ids\": ", out);
    jsonArray(out, sortedIssues(r.badIds), [&](const EntryIssue &e) {
        std::fputs("{\"id\": ", out);
        jsonString(out, e.id);
        std::fputs(", \"reason\": ", out);
        jsonString(out, e.reason);
        std::fprintf(out, ", \"line\": %d}", e.line);
    });

    std::fputs(",\n  \"cycles\": ", out);
    jsonArray(out, sortedCycles(r), [&](const Cycle &c) {
        std::fputs("{\"path\": ", out);
        jsonArray(out, c.path, [&](Class k) { jsonString(out, className(k)); });
        std::fprintf(out, ", \"failing\": %s, \"edges\": ", c.failing ? "true" : "false");
        jsonArray(out, c.edges, [&](const std::string &s) { jsonString(out, s); });
        std::fputc('}', out);
    });

    std::fputs(",\n  \"allowlisted\": {", out);
    bool first = true;
    for (const auto &[id, n] : r.allowlisted) {
        if (!first)
            std::fputs(", ", out);
        first = false;
        jsonString(out, id);
        std::fprintf(out, ": %ld", n);
    }
    std::fputc('}', out);

    std::fputs(",\n  \"consumer_reach\": ", out);
    jsonArray(out, sortedConsumers(r), [&](const ConsumerReach &c) {
        std::fputs("{\"from\": ", out);
        jsonString(out, c.from);
        std::fputs(", \"to\": ", out);
        jsonString(out, c.to);
        std::fputs(", \"to_class\": ", out);
        jsonString(out, className(c.toClass));
        std::fputc('}', out);
    });

    std::fputs(",\n  \"not_built\": ", out);
    jsonArray(out, sortedIssues(r.notBuilt), [&](const EntryIssue &e) { jsonIssue(out, e, false); });

    auto unresolved = r.unresolvedHeaders;
    std::sort(unresolved.begin(), unresolved.end());
    std::fputs(",\n  \"unresolved_headers\": ", out);
    jsonArray(out, unresolved, [&](const std::string &s) { jsonString(out, s); });

    std::fputs(",\n  \"metrics\": ", out);
    jsonArray(out, sortedMetrics(r), [&](const MetricRow &m) {
        const MetricCounts base = m.baseline.value_or(MetricCounts{});
        std::fputs("{\"class\": ", out);
        jsonString(out, m.cls);
        std::fprintf(out, ", \"refs\": %ld, \"refs_baseline\": %ld, \"distinct\": %ld, \"distinct_baseline\": %ld}",
                     m.refs, base.refs, m.distinct, base.distinct);
    });

    std::fprintf(out, ",\n  \"result\": \"%s\"\n}\n", r.exitCode() == kExitPass ? "PASS" : "FAIL");
}

} // namespace archcheck
