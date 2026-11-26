/**
 * Project: openRender
 *
 * File: closureRegistry.h
 *
 * Description:
 *   The closure registry (spec 035, contracts/closure-abi.md "Registry"): the C++ view of the entries a closure tree names by id. Process-wide;
 *   it fills itself with the eight built-ins on first use (through the call graph, no static registrar: gotcha 38), registration is under
 *   a mutex and lookup of a published entry takes no lock. Registration is open: later specs (036, 049) call registerClosure with no
 *   translation layer. Internal to Shading (not installed).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSUREREGISTRY_H
#define CLOSUREREGISTRY_H

#include <stdint.h>

#include "closureAbi.h"

namespace openrender::shading {

    // Registers an entry (the registry keeps its own copy of it and of its name and parameters) and returns its id; -1, with a diagnostic
    // naming the entry, for a short structSize, a missing or duplicate name (an alias counts), a BSDF entry without `ops`, a category out of
    // range, or a parameter whose offset + size overruns blockSize. The built-ins' own first-use fill is the only registration exempt from
    // "a BSDF needs ops": in Phase 1 their ops stay nullptr until Phase 2 fills them.
    int registerClosure(const OrClosureEntry &entry);

    // The entry named `name`; the aliases resolve (`emission` -> `uniform_edf`, with one deprecation message per process). An unknown name
    // returns nullptr with a diagnostic naming it; a null or empty name returns nullptr silently. The pointer stays valid for the process.
    const OrClosureEntry *findClosure(const char *name);

    // The entry of an id, or nullptr for an id the registry does not hold (a node's id is never negative here: MUL and ADD are not entries)
    const OrClosureEntry *closureEntry(int32_t id);

    // Interns a string parameter (the `microfacet` distribution): one stable pointer per content, owned by the registry
    const char *internClosureString(const char *text);

    // TEST HOOK, internal: how many times the registry has taken its registration lock. A lookup of a published entry never does, which
    // is what Closure_Registry_NoLock reads. Declared here and not in closureAbi.h so the plug-in interface carries no test hook.
    uint64_t closureRegistryLockCount();

} // namespace openrender::shading

#endif
