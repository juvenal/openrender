/**
 * Project: openRender
 *
 * File: pluginAbi.h
 *
 * Description:
 *   The one host-side plug-in version check and table check (spec 037, FR-004, FR-007; Infra,
 *   libRIBCommon). Internal: not installed.
 *
 *   checkPluginVersion resolves the family's version symbol in a loaded module, applies the verdict
 *   rules of data-model.md section 2, prints at most one diagnostic, and caches the verdict per
 *   (interface, realpath(path)) for the process, so a module loaded again prints nothing and a
 *   rejection is never retried. Thread-safe. The cache survives the module's unload.
 *
 *   checkPluginTable applies the table size rule of data-model.md section 3.
 *
 *   Diagnostics use CODE_VERSION (common/diagnostics.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef PLUGINABI_H
#define PLUGINABI_H

#include <cstddef>
#include <cstdint>

#include "orplugin.h"

// FR-007: the release in which unversioned plug-ins stop loading. The one constant the unversioned
// warning takes its release name from; the later release edits this line and the verdict rule.
#define OR_PLUGIN_UNVERSIONED_SUNSET "1.2.0"

struct SPluginFamily {
        const char *interfaceName; // "display", "shadeop", "dlo", "implicit", "dynamicload"
        const char *versionSymbol; // "orDisplayAbiVersion", ...
        uint32_t hostVersion;      // newest version the host implements
        const uint32_t *supported; // per supported MAJOR, the host's version of it (e.g. {1.0.0, 2.0.0})
        int numSupported;
        bool warnWhenUnversioned; // false only for dynamicload (FR-009)
};

enum class EPluginVerdict {
    Accept,
    AcceptNewerMinor,
    AcceptUnversioned,
    Reject
};

struct SPluginVersionVerdict {
        EPluginVerdict verdict;
        uint32_t pluginVersion; // as declared; 1.0.0 when the module declares none
        bool versioned;         // false when the symbol was absent
};

// The one routine (FR-004). `module` is an osLoadModule handle, `path` the file it came from.
SPluginVersionVerdict checkPluginVersion(const SPluginFamily &family, void *module, const char *path);

// The table size rule. Reads the 8-byte header at most before deciding and never reads past
// header.structSize. "The required minimum is the declared MAJOR's .0 table; fields added in a later
// MINOR are read only when structSize covers them." Rejects a table MAJOR that differs from the
// version symbol's. Diagnoses and returns false on rejection.
bool checkPluginTable(const SPluginFamily &family, const char *path, const OrTableHeader *table, size_t requiredSize, uint32_t versionFromSymbol);

#endif
