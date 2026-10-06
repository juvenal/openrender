/**
 * Project: openRender
 *
 * File: config.cpp
 *
 * Description:
 *   archcheck data-file parsers (see config.h).
 *
 *   API skeleton only (spec 022 T007): every parser reports "not
 *   implemented" so the parser unit tests are red. T008-T012 fill these in.
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

namespace archcheck {

namespace {

template <class T>
Parsed<T> notImplemented(std::string_view label) {
    Parsed<T> r;
    r.errors.push_back({std::string(label), 0, "not implemented"});
    return r;
}

} // namespace

std::optional<Class> classFromName(std::string_view) {
    return std::nullopt;
}

std::string_view className(Class) {
    return "?";
}

std::string Diagnostic::str() const {
    return label + ":" + std::to_string(line) + ": " + message;
}

std::optional<Class> DomainMap::classify(std::string_view) const {
    return std::nullopt;
}

Parsed<DomainMap> parseDomainMap(std::string_view, std::string_view label) {
    return notImplemented<DomainMap>(label);
}

bool RuleTable::allows(Class, Class) const {
    return false;
}

Parsed<RuleTable> parseRuleTable(std::string_view, std::string_view label) {
    return notImplemented<RuleTable>(label);
}

Parsed<std::vector<AllowlistEntry>> parseAllowlist(std::string_view, std::string_view label) {
    return notImplemented<std::vector<AllowlistEntry>>(label);
}

Parsed<std::map<std::string, RegistryEntry>> parseRegistry(std::string_view,
                                                           std::string_view label) {
    return notImplemented<std::map<std::string, RegistryEntry>>(label);
}

Parsed<std::map<std::string, MetricCounts>> parseMetricsBaseline(std::string_view,
                                                                 std::string_view label) {
    return notImplemented<std::map<std::string, MetricCounts>>(label);
}

} // namespace archcheck
