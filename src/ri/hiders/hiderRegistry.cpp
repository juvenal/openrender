/**
 * Project: openRender
 *
 * File: hiderRegistry.cpp
 *
 * Description:
 *   The hider registry. See hiderRegistry.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "hiderRegistry.h"

#include <cstring>
#include <mutex>
#include <vector>

#include "common/diagnostics.h"
#include "hiderAbi.hpp"

namespace {

    const uint32_t kSupportedMajors[] = {OR_HIDER_ABI_VERSION};

} // namespace

const SPluginFamily kHiderFamily = {"hider", "orHiderAbiVersion", OR_HIDER_ABI_VERSION, kSupportedMajors, 1, true};

CHiderRegistry::CHiderRegistry() : names_("Hider") {}

bool CHiderRegistry::add(const OrHiderTable *table, const char *origin) {
    if (table == nullptr || table->name == nullptr || table->name[0] == '\0') {
        error(CODE_BADTOKEN, "Hider registration without a name ignored (%s)\n", origin != nullptr ? origin : "(unknown origin)");
        return false;
    }

    // One version check (gotcha 27): the table's own version through the declared-version overload, then the size rule.
    const SPluginVersionVerdict verdict = checkPluginVersion(kHiderFamily, table->header.abiVersion, origin);
    if (verdict.verdict == EPluginVerdict::Reject)
        return false;
    if (!checkPluginTable(kHiderFamily, origin, &table->header, sizeof(OrHiderTable), table->header.abiVersion))
        return false;

    if (!names_.add(table, table->name, table->aliases, table->numAliases, origin))
        return false;

    // FR-014: the pair field of an alias is reserved. Warned about once, here; the alias is kept and selects the hider only.
    for (int i = 0; i < table->numAliases; ++i)
        if (table->aliases[i].integrator != nullptr)
            warning(CODE_BADTOKEN, "Hider alias \"%s\" names integrator \"%s\"; pair aliases are not supported yet, the alias selects the hider only\n", table->aliases[i].name, table->aliases[i].integrator);
    return true;
}

CHiderRegistry::SLookup CHiderRegistry::find(const char *nameOrAlias) const {
    const TKindNameTable<OrHiderTable, OrHiderAlias>::SLookup found = names_.find(nameOrAlias);
    return {found.table, found.alias};
}

int CHiderRegistry::size() const {
    return names_.size();
}

const OrHiderTable *CHiderRegistry::at(int i) const {
    return names_.at(i);
}

namespace {

    // The registrars that ran before, or after, the process registry was built. Defined here, once: a header-reached
    // static would give every shared-library consumer its own list (gotcha 22's class of defect).
    struct SPending {
            std::mutex mutex;
            std::vector<OrHiderTableGetter> getters;
            CHiderRegistry *registry = nullptr; // set once the process registry is built
    };

    SPending &pending() {
        static SPending *p = new SPending; // never destroyed: static registrars may run, and be read, around exit
        return *p;
    }

} // namespace

void orHiderAddPending(OrHiderTableGetter getter) {
    SPending &p = pending();
    std::lock_guard<std::mutex> lock(p.mutex);
    if (p.registry != nullptr)
        p.registry->add(getter(), "static registration"); // the registry is already built: register at once
    else
        p.getters.push_back(getter);
}

CHiderRegistry &hiderRegistry() {
    static CHiderRegistry *registry = new CHiderRegistry;
    static std::once_flag once;
    std::call_once(once, [] {
        registerBuiltinHiders(*registry);
        SPending &p = pending();
        std::lock_guard<std::mutex> lock(p.mutex);
        for (OrHiderTableGetter getter : p.getters)
            registry->add(getter(), "static registration");
        p.getters.clear();
        p.registry = registry;
    });
    return *registry;
}

const OrHiderTable *defaultHider() {
    return hiderRegistry().find("reyes").table;
}

bool hiderProvides(const OrHiderTable *table, const char *capability, uint32_t *version) {
    if (table == nullptr || capability == nullptr)
        return false;
    for (int i = 0; i < table->numProvides; ++i) {
        if (table->provides[i].name != nullptr && std::strcmp(table->provides[i].name, capability) == 0) {
            if (version != nullptr)
                *version = table->provides[i].version;
            return true;
        }
    }
    return false;
}
