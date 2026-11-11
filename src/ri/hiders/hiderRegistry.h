/**
 * Project: openRender
 *
 * File: hiderRegistry.h
 *
 * Description:
 *   The hider registry (spec 030, FR-001 to FR-008, FR-015, contracts/hider-registration.md "Registry API"). Names and aliases
 *   follow the shared TKindNameTable rules (common/kindRegistry.h). hiderRegistry() is the process instance: the built-in
 *   hiders first (raytrace, reyes, zbuffer, photon, in that order), then the static registrars (OR_REGISTER_HIDER) in append
 *   order, filled once. A local CHiderRegistry is what unit tests use for collisions and rejections. The renderer core reaches
 *   the built-ins only through this registry, which keeps them in a plain static link.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDERREGISTRY_H
#define HIDERREGISTRY_H

#include "common/kindRegistry.h"
#include "common/pluginAbi.h"
#include "hiderAbi.h"

// The plug-in family the version check applies to: interface "hider", MAJOR 2 (2.0.0) supported.
extern const SPluginFamily kHiderFamily;

class CHiderRegistry {
    public:
        CHiderRegistry();

        // Validates (an unnamed table, the version through checkPluginVersion's declared-version overload, the table size
        // through checkPluginTable, a name or alias collision) and stores. Returns false and reports one diagnostic when the
        // table is refused: nothing of a refused table is registered and the first registration stays. A non-NULL reserved
        // `integrator` field of an alias is warned about once and ignored; the alias is kept and selects the hider only.
        bool add(const OrHiderTable *table, const char *origin);

        struct SLookup {
                const OrHiderTable *table; // nullptr when nothing matches
                const OrHiderAlias *alias; // nullptr when the match is the table's own name
        };
        // Exact, case-sensitive; names and aliases both resolve.
        SLookup find(const char *nameOrAlias) const;

        // Enumeration for diagnostics and tests, in registration order.
        int size() const;
        const OrHiderTable *at(int i) const;

    private:
        TKindNameTable<OrHiderTable, OrHiderAlias> names_;
};

// The process instance: the built-ins, then the static registrars, built once (std::call_once).
CHiderRegistry &hiderRegistry();

// The `reyes` entry: what a frame uses for an unknown hider name.
const OrHiderTable *defaultHider();

// True when `table` provides `capability`; the provided version is written to *version when that is not nullptr. nullptr-safe.
bool hiderProvides(const OrHiderTable *table, const char *capability, uint32_t *version);

// The built-in tables, each defined in its hider's own source file.
const OrHiderTable *orHiderTableRaytrace();
const OrHiderTable *orHiderTableReyes();
const OrHiderTable *orHiderTableZbuffer();
const OrHiderTable *orHiderTablePhoton();

// Registers the four built-in hiders in the fixed order (builtinHiders.cpp).
void registerBuiltinHiders(CHiderRegistry &registry);

#endif
