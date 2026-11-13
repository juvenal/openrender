/**
 * Project: openRender
 *
 * File: integratorRegistry.cpp
 *
 * Description:
 *   The integrator registry. See integratorRegistry.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "transport/integratorRegistry.h"

#include <mutex>
#include <vector>

#include "common/diagnostics.h"
#include "transport/classicIntegrator.h"
#include "transport/integratorAbi.hpp"

namespace {

    const uint32_t kSupportedMajors[] = {OR_INTEGRATOR_ABI_VERSION};

} // namespace

const SPluginFamily kIntegratorFamily = {"integrator", "orIntegratorAbiVersion", OR_INTEGRATOR_ABI_VERSION, kSupportedMajors, 1, true};

CIntegratorRegistry::CIntegratorRegistry() : names_("Integrator") {}

bool CIntegratorRegistry::add(const OrIntegratorTable *table, const char *origin) {
    if (table == nullptr || table->name == nullptr || table->name[0] == '\0') {
        error(CODE_BADTOKEN, "Integrator registration without a name ignored (%s)\n", origin != nullptr ? origin : "(unknown origin)");
        return false;
    }

    // One version check (gotcha 27): the table's own version through the declared-version overload, then the size rule.
    const SPluginVersionVerdict verdict = checkPluginVersion(kIntegratorFamily, table->header.abiVersion, origin);
    if (verdict.verdict == EPluginVerdict::Reject)
        return false;
    if (!checkPluginTable(kIntegratorFamily, origin, &table->header, sizeof(OrIntegratorTable), table->header.abiVersion))
        return false;

    return names_.add(table, table->name, table->aliases, table->numAliases, origin);
}

CIntegratorRegistry::SLookup CIntegratorRegistry::find(const char *nameOrAlias) const {
    const TKindNameTable<OrIntegratorTable, OrIntegratorAlias>::SLookup found = names_.find(nameOrAlias);
    return {found.table, found.alias};
}

int CIntegratorRegistry::size() const {
    return names_.size();
}

const OrIntegratorTable *CIntegratorRegistry::at(int i) const {
    return names_.at(i);
}

namespace {

    // The registrars that ran before, or after, the process registry was built. Defined here, once: a header-reached
    // static would give every shared-library consumer its own list (gotcha 22's class of defect).
    struct SPending {
            std::mutex mutex;
            std::vector<OrIntegratorTableGetter> getters;
            CIntegratorRegistry *registry = nullptr; // set once the process registry is built
    };

    SPending &pending() {
        static SPending *p = new SPending; // never destroyed: static registrars may run, and be read, around exit
        return *p;
    }

} // namespace

void orIntegratorAddPending(OrIntegratorTableGetter getter) {
    SPending &p = pending();
    std::lock_guard<std::mutex> lock(p.mutex);
    if (p.registry != nullptr)
        p.registry->add(getter(), "static registration"); // the registry is already built: register at once
    else
        p.getters.push_back(getter);
}

CIntegratorRegistry &integratorRegistry() {
    static CIntegratorRegistry *registry = new CIntegratorRegistry;
    static std::once_flag once;
    std::call_once(once, [] {
        registerBuiltinIntegrators(*registry);
        SPending &p = pending();
        std::lock_guard<std::mutex> lock(p.mutex);
        for (OrIntegratorTableGetter getter : p.getters)
            registry->add(getter(), "static registration");
        p.getters.clear();
        p.registry = registry;
    });
    return *registry;
}

const OrIntegratorTable *defaultIntegrator() {
    return integratorRegistry().find("classic").table;
}
