/**
 * Project: openRender
 *
 * File: config.cpp
 *
 * Description:
 *   archcheck data-file parsers (see config.h) and path mapping.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "config.h"

#include <cctype>
#include <charconv>
#include <set>
#include <system_error>
#include <tuple>

namespace archcheck {

    namespace {

        constexpr std::array<std::string_view, kClassCount> kClassNames = {
            "SceneInterface",
            "GraphicsState",
            "Geometry",
            "Hiders",
            "Shading",
            "Texturing",
            "ShaderDSO",
            "Procedures",
            "Display",
            "Core",
            "Infra",
            "Consumer",
        };

        std::size_t idx(Class c) {
            return static_cast<std::size_t>(c);
        }

        // Classes that take part in the acyclicity check: the nine domains plus Infra.
        bool isLayered(Class c) {
            return c != Class::Core && c != Class::Consumer;
        }

        // Splits text into lines, calling fn(lineNumber, line) without the newline.
        template <class Fn>
        void forEachLine(std::string_view text, Fn fn) {
            int lineNo = 0;
            while (!text.empty()) {
                lineNo++;
                const std::size_t nl = text.find('\n');
                std::string_view line = text.substr(0, nl);
                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);
                fn(lineNo, line);
                if (nl == std::string_view::npos)
                    break;
                text.remove_prefix(nl + 1);
            }
        }

        // Whitespace-separated fields of a data-file line, '#' comment removed.
        std::vector<std::string_view> fields(std::string_view line) {
            if (const std::size_t hash = line.find('#'); hash != std::string_view::npos)
                line = line.substr(0, hash);
            std::vector<std::string_view> out;
            std::size_t i = 0;
            while (i < line.size()) {
                while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
                    i++;
                const std::size_t start = i;
                while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
                    i++;
                if (i > start)
                    out.push_back(line.substr(start, i - start));
            }
            return out;
        }

        // "DB-" followed by at least three digits, and nothing else.
        bool isRegistryId(std::string_view s) {
            if (s.size() < 6 || s.substr(0, 3) != "DB-")
                return false;
            for (const char c : s.substr(3))
                if (!std::isdigit(static_cast<unsigned char>(c)))
                    return false;
            return true;
        }

        bool isWordChar(char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        std::optional<long> parseCount(std::string_view s) {
            long v = 0;
            const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
            if (ec != std::errc() || end != s.data() + s.size() || v < 0)
                return std::nullopt;
            return v;
        }

        class Errors {
            public:
                explicit Errors(std::string_view label) : label_(label) {}

                void add(int line, std::string message) { list_.push_back({label_, line, std::move(message)}); }

                template <class T>
                Parsed<T> finish(T value) {
                    Parsed<T> r;
                    r.errors = std::move(list_);
                    if (r.errors.empty())
                        r.value = std::move(value);
                    return r;
                }

            private:
                std::string label_;
                std::vector<Diagnostic> list_;
        };

        // Returns a cycle among the layered classes as "A -> B -> A", or "".
        std::string findCycle(const RuleTable &t) {
            enum Mark { White,
                        Grey,
                        Black };
            std::array<Mark, kClassCount> mark{};
            std::vector<std::size_t> stack;
            std::string cycle;

            auto visit = [&](auto &&self, std::size_t u) -> bool {
                mark[u] = Grey;
                stack.push_back(u);
                for (std::size_t v = 0; v < kClassCount; v++) {
                    if (v == u || !t.allowed[u][v] || !isLayered(static_cast<Class>(v)))
                        continue;
                    if (mark[v] == Grey) {
                        std::size_t i = 0;
                        while (stack[i] != v)
                            i++;
                        for (; i < stack.size(); i++)
                            cycle += std::string(kClassNames[stack[i]]) + " -> ";
                        cycle += kClassNames[v];
                        return true;
                    }
                    if (mark[v] == White && self(self, v))
                        return true;
                }
                stack.pop_back();
                mark[u] = Black;
                return false;
            };
            for (std::size_t u = 0; u < kClassCount; u++)
                if (isLayered(static_cast<Class>(u)) && mark[u] == White && visit(visit, u))
                    return cycle;
            return {};
        }

    } // namespace

    std::optional<Class> classFromName(std::string_view name) {
        for (std::size_t i = 0; i < kClassCount; i++)
            if (kClassNames[i] == name)
                return static_cast<Class>(i);
        return std::nullopt;
    }

    std::string_view className(Class c) {
        return kClassNames[idx(c)];
    }

    std::string Diagnostic::str() const {
        return label + ":" + std::to_string(line) + ": " + message;
    }

    // ---------------------------------------------------------------------------
    // domain-map.txt
    // ---------------------------------------------------------------------------

    std::optional<Class> DomainMap::classify(std::string_view path) const {
        const MapRule *best = nullptr;
        for (const MapRule &r : rules) {
            if (r.kind == MapKind::File) {
                if (r.pattern == path)
                    return r.cls;
            }
            else if (path.substr(0, r.pattern.size()) == r.pattern &&
                     (best == nullptr || r.pattern.size() > best->pattern.size())) {
                best = &r;
            }
        }
        if (best == nullptr)
            return std::nullopt;
        return best->cls;
    }

    Parsed<DomainMap> parseDomainMap(std::string_view text, std::string_view label) {
        Errors errors(label);
        DomainMap map;
        std::map<std::string, int, std::less<>> seen;

        forEachLine(text, [&](int lineNo, std::string_view line) {
            const auto f = fields(line);
            if (f.empty())
                return;
            if (f.size() != 3) {
                errors.add(lineNo, "expected 3 fields `<kind> <pattern> <class>`, got " +
                                       std::to_string(f.size()));
                return;
            }
            MapRule rule;
            rule.line = lineNo;
            if (f[0] == "prefix")
                rule.kind = MapKind::Prefix;
            else if (f[0] == "file")
                rule.kind = MapKind::File;
            else {
                errors.add(lineNo, "unknown rule kind '" + std::string(f[0]) + "' (prefix | file)");
                return;
            }
            const auto cls = classFromName(f[2]);
            if (!cls) {
                errors.add(lineNo, "unknown class '" + std::string(f[2]) + "'");
                return;
            }
            rule.cls = *cls;
            rule.pattern = std::string(f[1]);
            if (const auto it = seen.find(rule.pattern); it != seen.end()) {
                errors.add(lineNo, "duplicate pattern '" + rule.pattern + "' (first at line " +
                                       std::to_string(it->second) + ")");
                return;
            }
            seen.emplace(rule.pattern, lineNo);
            map.rules.push_back(std::move(rule));
        });
        return errors.finish(std::move(map));
    }

    // ---------------------------------------------------------------------------
    // domain-rules.txt
    // ---------------------------------------------------------------------------

    bool RuleTable::allows(Class from, Class to) const {
        return from == to || allowed[idx(from)][idx(to)];
    }

    Parsed<RuleTable> parseRuleTable(std::string_view text, std::string_view label) {
        Errors errors(label);
        RuleTable table;
        std::array<int, kClassCount> rowLine{};
        bool cycleReported = false;

        forEachLine(text, [&](int lineNo, std::string_view line) {
            const auto f = fields(line);
            if (f.empty())
                return;
            if (f.size() < 2 || f[1] != "->") {
                errors.add(lineNo, "expected `<class> -> <class>...`");
                return;
            }

            // Resolve the left side ("*" = every class) and the right side.
            std::vector<Class> from;
            if (f[0] == "*") {
                for (std::size_t i = 0; i < kClassCount; i++)
                    from.push_back(static_cast<Class>(i));
            }
            else if (const auto c = classFromName(f[0])) {
                if (rowLine[idx(*c)] != 0) {
                    errors.add(lineNo, "duplicate row for " + std::string(f[0]) + " (first at line " +
                                           std::to_string(rowLine[idx(*c)]) + ")");
                    return;
                }
                rowLine[idx(*c)] = lineNo;
                from.push_back(*c);
            }
            else {
                errors.add(lineNo, "unknown class '" + std::string(f[0]) + "'");
                return;
            }

            std::vector<Class> to;
            bool bad = false;
            for (std::size_t i = 2; i < f.size(); i++) {
                if (f[i] == "*") {
                    for (std::size_t k = 0; k < kClassCount; k++)
                        to.push_back(static_cast<Class>(k));
                }
                else if (const auto c = classFromName(f[i])) {
                    to.push_back(*c);
                }
                else {
                    errors.add(lineNo, "unknown class '" + std::string(f[i]) + "'");
                    bad = true;
                }
            }
            if (bad)
                return;

            for (const Class a : from) {
                for (const Class b : to) {
                    if (a == b)
                        continue;
                    // Infra is the shared floor: it may depend on nothing.
                    if (a == Class::Infra) {
                        errors.add(lineNo, "Infra may depend on no other class (Infra -> " +
                                               std::string(className(b)) + ")");
                        return;
                    }
                    // Core is the composition root: only Consumer may reach it.
                    if (b == Class::Core && a != Class::Consumer) {
                        errors.add(lineNo, "only Consumer may depend on Core (" +
                                               std::string(className(a)) + " -> Core)");
                        return;
                    }
                    table.allowed[idx(a)][idx(b)] = true;
                }
            }

            if (!cycleReported) {
                if (const std::string cycle = findCycle(table); !cycle.empty()) {
                    errors.add(lineNo, "allowed-dependency rules form a cycle: " + cycle);
                    cycleReported = true;
                }
            }
        });
        return errors.finish(std::move(table));
    }

    // ---------------------------------------------------------------------------
    // allowlist.txt
    // ---------------------------------------------------------------------------

    Parsed<std::vector<AllowlistEntry>> parseAllowlist(std::string_view text, std::string_view label) {
        Errors errors(label);
        std::vector<AllowlistEntry> entries;
        // (kind, from, to, platform) -> line, and (kind, from, to) -> lines by taggedness.
        std::map<std::tuple<EdgeKind, std::string, std::string, int>, int> seen;
        std::map<std::tuple<EdgeKind, std::string, std::string>, std::pair<int, int>> tagging;

        forEachLine(text, [&](int lineNo, std::string_view line) {
            const auto f = fields(line);
            if (f.empty())
                return;
            if ((f.size() != 5 && f.size() != 6) || f[3] != "->") {
                errors.add(lineNo, "expected `<id> <kind> <from> -> <to> [macos|linux]`");
                return;
            }
            AllowlistEntry e;
            e.line = lineNo;
            if (!isRegistryId(f[0])) {
                errors.add(lineNo, "bad registry id '" + std::string(f[0]) + "' (DB- plus 3+ digits)");
                return;
            }
            e.registryId = std::string(f[0]);
            if (f[1] == "include")
                e.kind = EdgeKind::Include;
            else if (f[1] == "link")
                e.kind = EdgeKind::Link;
            else {
                errors.add(lineNo, "unknown edge kind '" + std::string(f[1]) + "' (include | link)");
                return;
            }
            e.fromFile = std::string(f[2]);
            e.to = std::string(f[4]);
            if (f.size() == 6) {
                if (f[5] == "macos")
                    e.platform = Platform::MacOS;
                else if (f[5] == "linux")
                    e.platform = Platform::Linux;
                else {
                    errors.add(lineNo, "unknown platform tag '" + std::string(f[5]) + "' (macos | linux)");
                    return;
                }
            }

            const int plat = e.platform ? static_cast<int>(*e.platform) + 1 : 0;
            const auto key = std::make_tuple(e.kind, e.fromFile, e.to, plat);
            if (const auto it = seen.find(key); it != seen.end()) {
                errors.add(lineNo, "duplicate entry (first at line " + std::to_string(it->second) + ")");
                return;
            }
            seen.emplace(key, lineNo);

            auto &[untaggedLine, taggedLine] = tagging[std::make_tuple(e.kind, e.fromFile, e.to)];
            const int other = e.platform ? untaggedLine : taggedLine;
            if (other != 0) {
                errors.add(lineNo, "same edge listed tagged and untagged (other at line " +
                                       std::to_string(other) + ")");
                return;
            }
            (e.platform ? taggedLine : untaggedLine) = lineNo;
            entries.push_back(std::move(e));
        });
        return errors.finish(std::move(entries));
    }

    // ---------------------------------------------------------------------------
    // Registry headings
    // ---------------------------------------------------------------------------

    Parsed<std::map<std::string, RegistryEntry>> parseRegistry(std::string_view text,
                                                               std::string_view label) {
        Errors errors(label);
        std::map<std::string, RegistryEntry> entries;
        RegistryStatus status = RegistryStatus::Open;

        forEachLine(text, [&](int lineNo, std::string_view line) {
            std::string_view trimmed = line;
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))
                trimmed.remove_suffix(1);
            if (trimmed == "## Resolved") {
                status = RegistryStatus::Resolved;
                return;
            }
            if (line.substr(0, 7) != "### DB-")
                return;
            std::size_t end = 7;
            while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end])))
                end++;
            if (end - 7 < 3 || (end < line.size() && isWordChar(line[end])))
                return;
            const std::string id(line.substr(4, end - 4));
            if (const auto it = entries.find(id); it != entries.end()) {
                errors.add(lineNo,
                           "duplicate registry id " + id + " (first at line " + std::to_string(it->second.line) + ")");
                return;
            }
            entries.emplace(id, RegistryEntry{id, status, lineNo});
        });
        return errors.finish(std::move(entries));
    }

    // ---------------------------------------------------------------------------
    // metrics-baseline.txt
    // ---------------------------------------------------------------------------

    Parsed<std::map<std::string, MetricCounts>> parseMetricsBaseline(std::string_view text,
                                                                     std::string_view label) {
        Errors errors(label);
        std::map<std::string, MetricCounts> baseline;
        std::map<std::string, int> seen;

        forEachLine(text, [&](int lineNo, std::string_view line) {
            const auto f = fields(line);
            if (f.empty())
                return;
            if (f.size() != 3) {
                errors.add(lineNo, "expected 3 fields `<class> <refs> <distinct>`, got " +
                                       std::to_string(f.size()));
                return;
            }
            const std::string name(f[0]);
            if (name != "TOTAL" && !classFromName(name)) {
                errors.add(lineNo, "unknown class '" + name + "'");
                return;
            }
            const auto refs = parseCount(f[1]);
            const auto distinct = parseCount(f[2]);
            if (!refs || !distinct) {
                errors.add(lineNo, "counts must be non-negative integers, got '" + std::string(f[1]) +
                                       "' '" + std::string(f[2]) + "'");
                return;
            }
            if (const auto it = seen.find(name); it != seen.end()) {
                errors.add(lineNo, "duplicate class " + name + " (first at line " +
                                       std::to_string(it->second) + ")");
                return;
            }
            seen.emplace(name, lineNo);
            baseline.emplace(name, MetricCounts{*refs, *distinct});
        });
        return errors.finish(std::move(baseline));
    }

    // ---------------------------------------------------------------------------
    // Path mapping
    // ---------------------------------------------------------------------------

    namespace {

        std::string canonicalDir(const std::filesystem::path &p) {
            std::error_code ec;
            std::string s = std::filesystem::weakly_canonical(std::filesystem::absolute(p), ec).generic_string();
            if (ec)
                s = std::filesystem::absolute(p).lexically_normal().generic_string();
            if (s.empty() || s.back() != '/')
                s += '/';
            return s;
        }

    } // namespace

    PathMapper::PathMapper(const std::filesystem::path &sourceDir, const std::filesystem::path &buildDir)
        : sourceDir_(canonicalDir(sourceDir)), buildDir_(canonicalDir(buildDir)) {}

    std::optional<std::string> PathMapper::map(std::string_view path) const {
        const std::string key(path);
        if (const auto it = cache_.find(key); it != cache_.end())
            return it->second;

        std::filesystem::path p(key);
        if (p.is_relative())
            p = std::filesystem::path(buildDir_) / p;
        std::error_code ec;
        std::string canon = std::filesystem::weakly_canonical(p, ec).generic_string();
        if (ec)
            canon = p.lexically_normal().generic_string();

        // The build tree may live inside the source tree, so test it first.
        std::optional<std::string> mapped;
        if (canon.rfind(buildDir_, 0) == 0)
            mapped = "@build/" + canon.substr(buildDir_.size());
        else if (canon.rfind(sourceDir_, 0) == 0)
            mapped = canon.substr(sourceDir_.size());
        cache_.emplace(key, mapped);
        return mapped;
    }

} // namespace archcheck
