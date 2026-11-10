/**
 * Project: openRender
 *
 * File: integratorRegistry.h
 *
 * Description:
 *   The integrator registry (spec 030, FR-010, FR-012, FR-015, contracts/integrator-interface.md "Registry API"). It
 *   mirrors the hider registry over the shared TKindNameTable rules (common/kindRegistry.h): names and aliases are
 *   exact and case-sensitive, enumeration is in registration order, the first registration stays and a refusal is of
 *   the whole table. integratorRegistry() is the process instance: the built-in integrators first, then the static
 *   registrars (OR_REGISTER_INTEGRATOR) in append order, filled once. A local CIntegratorRegistry is what unit tests use
 *   for collisions and rejections.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRANSPORT_INTEGRATORREGISTRY_H
#define TRANSPORT_INTEGRATORREGISTRY_H

#include "common/kindRegistry.h"
#include "common/pluginAbi.h"
#include "transport/integratorAbi.h"

// The plug-in family the version check applies to: interface "integrator", MAJOR 2 (2.0.0) supported.
extern const SPluginFamily kIntegratorFamily;

class CIntegratorRegistry {
    public:
        CIntegratorRegistry();

        // Validates (an unnamed table, the version through checkPluginVersion's declared-version overload, the table size
        // through checkPluginTable, a name or alias collision) and stores. Returns false and reports one diagnostic when the
        // table is refused: nothing of a refused table is registered, and the first registration stays. `origin` names the
        // registration in the diagnostics.
        bool add(const OrIntegratorTable *table, const char *origin);

        struct SLookup {
                const OrIntegratorTable *table; // nullptr when nothing matches
                const OrIntegratorAlias *alias; // nullptr when the match is the table's own name
        };
        // Exact, case-sensitive; names and aliases both resolve.
        SLookup find(const char *nameOrAlias) const;

        // Enumeration for diagnostics and tests, in registration order.
        int size() const;
        const OrIntegratorTable *at(int i) const;

    private:
        TKindNameTable<OrIntegratorTable, OrIntegratorAlias> names_;
};

// The process instance: `classic`, then the static registrars, built once (std::call_once).
CIntegratorRegistry &integratorRegistry();

// The `classic` table: what a frame uses when no Integrator statement was given and the fallback for an unknown name.
const OrIntegratorTable *defaultIntegrator();

// Registers the built-in integrators (builtinIntegrators.cpp): `classic`.
void registerBuiltinIntegrators(CIntegratorRegistry &registry);

#endif
