/**
 * Project: openRender
 *
 * File: config.h
 *
 * Description:
 *   archcheck configuration model and data-file parsers: the Principle VIII
 *   classes, the domain map, the allowed-dependency rule table, the allowlist,
 *   the registry headings and the metric baseline. Formats are fixed by
 *   specs/022-domain-dependency-guardrails/contracts/data-files.md; entities by
 *   data-model.md.
 *
 *   Every parser takes the file text plus a label (normally the file path)
 *   and returns its errors as values; each error carries "label:line". Only
 *   main() turns errors into an exit code.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ARCHCHECK_CONFIG_H
#define ARCHCHECK_CONFIG_H

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace archcheck {

// The ownership category of a file (data-model.md › Class).
enum class Class {
    SceneInterface,
    GraphicsState,
    Geometry,
    Hiders,
    Shading,
    Texturing,
    ShaderDSO,
    Procedures,
    Display,
    Core,
    Infra,
    Consumer,
};
inline constexpr std::size_t kClassCount = 12;

std::optional<Class> classFromName(std::string_view name);
std::string_view className(Class c);

// One parse error, rendered as "label:line: message".
struct Diagnostic {
    std::string label;
    int line = 0;
    std::string message;

    std::string str() const;
};

// A parse result: a value, or the errors that prevented it.
template <class T>
struct Parsed {
    std::optional<T> value;
    std::vector<Diagnostic> errors;

    bool ok() const { return value.has_value() && errors.empty(); }
};

// ---------------------------------------------------------------------------
// domain-map.txt
// ---------------------------------------------------------------------------

enum class MapKind { Prefix, File };

struct MapRule {
    MapKind kind = MapKind::Prefix;
    std::string pattern; // repo-relative, or @build/-relative for generated sources
    Class cls = Class::Infra;
    int line = 0;
};

struct DomainMap {
    std::vector<MapRule> rules;

    // A file rule beats a prefix rule; among prefixes the longest match wins.
    std::optional<Class> classify(std::string_view path) const;
};

Parsed<DomainMap> parseDomainMap(std::string_view text, std::string_view label);

// ---------------------------------------------------------------------------
// domain-rules.txt
// ---------------------------------------------------------------------------

struct RuleTable {
    // allowed[from][to]; same-class edges are always allowed.
    std::array<std::array<bool, kClassCount>, kClassCount> allowed{};

    bool allows(Class from, Class to) const;
};

// Rejects a cyclic domain+Infra subgraph and any Infra -> X entry.
Parsed<RuleTable> parseRuleTable(std::string_view text, std::string_view label);

// ---------------------------------------------------------------------------
// allowlist.txt
// ---------------------------------------------------------------------------

enum class EdgeKind { Include, Link };
enum class Platform { MacOS, Linux };

struct AllowlistEntry {
    std::string registryId; // DB- + at least 3 digits
    EdgeKind kind = EdgeKind::Include;
    std::string fromFile;
    std::string to; // resolved header path, or mangled symbol without the macOS '_'
    std::optional<Platform> platform; // absent = both platforms
    int line = 0;
};

Parsed<std::vector<AllowlistEntry>> parseAllowlist(std::string_view text, std::string_view label);

// ---------------------------------------------------------------------------
// DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md (headings only)
// ---------------------------------------------------------------------------

enum class RegistryStatus { Open, Resolved };

struct RegistryEntry {
    std::string id;
    RegistryStatus status = RegistryStatus::Open;
    int line = 0;
};

// `### DB-NNN` headings before `## Resolved` are Open, after it Resolved.
Parsed<std::map<std::string, RegistryEntry>> parseRegistry(std::string_view text,
                                                           std::string_view label);

// ---------------------------------------------------------------------------
// metrics-baseline.txt
// ---------------------------------------------------------------------------

struct MetricCounts {
    long refs = 0;
    long distinct = 0;
};

// Keyed by class name, plus "TOTAL".
Parsed<std::map<std::string, MetricCounts>> parseMetricsBaseline(std::string_view text,
                                                                 std::string_view label);

} // namespace archcheck

#endif // ARCHCHECK_CONFIG_H
