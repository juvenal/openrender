/**
 * Project: openRender
 *
 * File: kindRegistry.h
 *
 * Description:
 *   TKindNameTable: the name and alias rules the hider registry (Hiders) and the integrator registry (Transport)
 *   share (spec 030, FR-010, FR-012, FR-015). Infra, header-only, includes nothing from a domain: both registries
 *   instantiate it, so the two domains share rules without sharing code across a boundary.
 *
 *   The rules:
 *     - a table's name must be non-empty;
 *     - a name must be unique across all names and aliases, and so must an alias (a hider and an integrator may
 *       share a name: each registry has its own table);
 *     - matching is exact and case-sensitive;
 *     - enumeration is in registration order;
 *     - the first registration stays; a refusal is of the whole table (a name or alias that collides, an unnamed
 *       table or alias): nothing of it is registered, no alias without its target.
 *
 *   `Alias` is any type with a `const char *name` member (OrHiderAlias, OrIntegratorAlias). The table keeps the
 *   pointers it is given: tables and aliases are static and outlive the registry (orplugin.h rule 4). Not
 *   thread-safe: a registry is filled once, before it is read, or under the owner's lock.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef KINDREGISTRY_H
#define KINDREGISTRY_H

#include <string>
#include <vector>

#include "diagnostics.h"

template <class Table, class Alias>
class TKindNameTable {
    public:
        struct SLookup {
                const Table *table; // nullptr when nothing matches
                const Alias *alias; // nullptr when the match is a table's own name
        };

        // `kind` is the word the diagnostics use: "Hider" or "Integrator".
        explicit TKindNameTable(const char *kind) : kind_(kind) {}

        // Registers `table` under `name` and `aliases`. Returns false, after one diagnostic, when the table is refused.
        bool add(const Table *table, const char *name, const Alias *aliases, int numAliases, const char *origin) {
            const char *shown = origin != nullptr ? origin : "(unknown origin)";
            if (name == nullptr || name[0] == '\0') {
                error(CODE_BADTOKEN, "%s registration without a name ignored (%s)\n", kind_, shown);
                return false;
            }

            // Every name the table brings, checked against what is registered and against the table's own earlier names.
            std::vector<SName> incoming;
            incoming.push_back({name, table, nullptr});
            for (int i = 0; i < numAliases; ++i) {
                const char *aliasName = aliases[i].name;
                if (aliasName == nullptr || aliasName[0] == '\0') {
                    error(CODE_BADTOKEN, "%s \"%s\" has an alias without a name; the registration is ignored (%s)\n", kind_, name, shown);
                    return false;
                }
                incoming.push_back({aliasName, table, &aliases[i]});
            }

            for (size_t i = 0; i < incoming.size(); ++i) {
                const SName *owner = nullptr;
                for (const SName &existing : names_)
                    if (existing.name == incoming[i].name) {
                        owner = &existing;
                        break;
                    }
                const char *ownerName = owner != nullptr ? tableName_[owner->tableIndex].c_str() : nullptr;
                std::string ownerText;
                if (owner == nullptr)
                    for (size_t j = 0; j < i; ++j)
                        if (incoming[j].name == incoming[i].name) {
                            ownerText = name; // a collision inside the table itself: it is the first registration of this table
                            ownerName = ownerText.c_str();
                            break;
                        }
                if (ownerName != nullptr) {
                    error(CODE_BADTOKEN, "%s \"%s\" is already registered (by \"%s\"); the second registration is ignored\n", kind_, incoming[i].name.c_str(), ownerName);
                    return false;
                }
            }

            const size_t index = tables_.size();
            tables_.push_back(table);
            tableName_.push_back(name);
            for (const SName &n : incoming)
                names_.push_back({n.name, n.table, n.alias, index});
            return true;
        }

        // Exact, case-sensitive; names and aliases both resolve. nullptr and "" match nothing.
        SLookup find(const char *nameOrAlias) const {
            if (nameOrAlias == nullptr || nameOrAlias[0] == '\0')
                return {nullptr, nullptr};
            for (const SName &n : names_)
                if (n.name == nameOrAlias)
                    return {n.table, n.alias};
            return {nullptr, nullptr};
        }

        // Enumeration, in registration order (one entry per table).
        int size() const { return (int)tables_.size(); }
        const Table *at(int i) const { return (i >= 0 && i < size()) ? tables_[i] : nullptr; }

    private:
        struct SName {
                std::string name;
                const Table *table;
                const Alias *alias;
                size_t tableIndex = 0;
        };

        const char *kind_;
        std::vector<const Table *> tables_;
        std::vector<std::string> tableName_;
        std::vector<SName> names_;
};

#endif
