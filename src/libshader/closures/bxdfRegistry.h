/**
 * Project: openRender
 *
 * File: bxdfRegistry.h
 *
 * Description:
 *   The Bxdf registry (spec 035, contracts/bxdf.md "Registry"). registerBxdf() checks a table through checkPluginVersion's in-process
 *   overload (the one version check, gotcha 27) and the table-size rule; a refused table is recorded with its reason so that a later statement
 *   names it; a duplicate name refuses the whole table. The four built-ins (builtinBxdfs.cpp) register on the first use of the registry,
 *   through the call graph: there is no static registrar, so a static link needs no anchor (gotcha 38).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef BXDFREGISTRY_H
#define BXDFREGISTRY_H

#include <stdint.h>

#include "bxdfAbi.h"

namespace openrender::shading {

    enum class BxdfLookupReason : int {
        Found,
        PrefixReserved, // "sl:" and "osl:" are reserved for RSL 2.0 / OSL Bxdfs (specs 036, 049)
        Unknown,
        VersionRefused // the table was refused at registration for its MAJOR; `major` carries it
    };

    struct SBxdfLookup {
            const OrBxdfTable *table = nullptr;
            BxdfLookupReason reason = BxdfLookupReason::Unknown;
            uint32_t major = 0; // VersionRefused only
    };

    // false (with a diagnostic) when the table is refused: a version the host does not support (recorded), a table smaller than its header says,
    // a missing name, or a name already registered (the first stays)
    bool registerBxdf(const OrBxdfTable *table, const char *origin);

    SBxdfLookup findBxdf(const char *name);

    // The four built-ins' tables (builtinBxdfs.cpp)
    const OrBxdfTable *diffuseBxdfTable();
    const OrBxdfTable *plasticBxdfTable();
    const OrBxdfTable *conductorBxdfTable();
    const OrBxdfTable *dielectricBxdfTable();

} // namespace openrender::shading

#endif
