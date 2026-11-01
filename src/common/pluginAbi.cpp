/**
 * Project: openRender
 *
 * File: pluginAbi.cpp
 *
 * Description:
 *   The one host-side plug-in version check and table check. See pluginAbi.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "pluginAbi.h"

#include <cstdlib>
#include <map>
#include <mutex>
#include <string>

#include "diagnostics.h"
#include "os.h"

namespace {

    typedef uint32_t (*VersionFunction)(void);

    // Keyed by interface and canonical path. Never destroyed: a verdict must outlive the module's
    // unload and any static destruction order at exit.
    struct SVerdictCache {
            std::mutex mutex;
            std::map<std::string, SPluginVersionVerdict> verdicts;
    };

    SVerdictCache &cache() {
        static SVerdictCache *c = new SVerdictCache;
        return *c;
    }

    std::string canonicalPath(const char *path) {
#ifdef _WINDOWS
        return path != nullptr ? path : "";
#else
        if (path == nullptr)
            return "";
        // realpath allocates the result: a fixed OS_MAX_PATH_LENGTH buffer is smaller than PATH_MAX, which
        // glibc's fortified realpath aborts on ("buffer overflow detected") before looking at the path.
        char *resolved = realpath(path, nullptr);
        std::string text = resolved != nullptr ? std::string(resolved) : std::string(path);
        free(resolved);
        return text;
#endif
    }

    std::string describe(uint32_t v) {
        return std::to_string(OR_ABI_MAJOR(v)) + "." + std::to_string(OR_ABI_MINOR(v)) + "." + std::to_string(OR_ABI_PATCH(v));
    }

    std::string supportedMajors(const SPluginFamily &family) {
        std::string text;
        for (int i = 0; i < family.numSupported; ++i) {
            if (i != 0)
                text += ", ";
            text += std::to_string(OR_ABI_MAJOR(family.supported[i]));
        }
        return text;
    }

    // The host's version of `major`, or 0 when the host does not support it.
    bool hostVersionOf(const SPluginFamily &family, uint32_t major, uint32_t &version) {
        for (int i = 0; i < family.numSupported; ++i) {
            if (OR_ABI_MAJOR(family.supported[i]) == major) {
                version = family.supported[i];
                return true;
            }
        }
        return false;
    }

} // namespace

SPluginVersionVerdict checkPluginVersion(const SPluginFamily &family, void *module, const char *path) {
    SVerdictCache &c = cache();
    // One lock around lookup, computation and diagnostic: a verdict is computed and announced once.
    std::lock_guard<std::mutex> lock(c.mutex);

    const std::string key = std::string(family.interfaceName) + "\n" + canonicalPath(path);
    const auto found = c.verdicts.find(key);
    if (found != c.verdicts.end())
        return found->second;

    const char *shown = path != nullptr ? path : "(unknown path)";
    SPluginVersionVerdict v = {EPluginVerdict::Accept, OR_ABI_VERSION(1, 0, 0), false};
    const VersionFunction query = module != nullptr ? (VersionFunction)osResolve(module, family.versionSymbol) : nullptr;

    if (query == nullptr) {
        // No version symbol: a module built before the interface was versioned is 1.0.0.
        if (family.warnWhenUnversioned) {
            v.verdict = EPluginVerdict::AcceptUnversioned;
            warning(CODE_VERSION, "%s: the %s plug-in declares no interface version; it is treated as 1.0.0. Unversioned plug-ins will be rejected from openRender %s: rebuild against the installed header and declare the version.\n", shown, family.interfaceName, OR_PLUGIN_UNVERSIONED_SUNSET);
        }
    }
    else {
        v.versioned = true;
        v.pluginVersion = query();
        uint32_t hostForMajor = 0;
        if (!hostVersionOf(family, OR_ABI_MAJOR(v.pluginVersion), hostForMajor)) {
            v.verdict = EPluginVerdict::Reject;
            error(CODE_VERSION, "%s: the %s plug-in is interface version %s (0x%08x), which this openRender (%s version %s, supported major version(s): %s) cannot load: rebuild against the installed header.\n", shown, family.interfaceName, describe(v.pluginVersion).c_str(), (unsigned)v.pluginVersion, family.interfaceName, describe(family.hostVersion).c_str(), supportedMajors(family).c_str());
        }
        else if (OR_ABI_MINOR(v.pluginVersion) > OR_ABI_MINOR(hostForMajor)) {
            v.verdict = EPluginVerdict::AcceptNewerMinor;
            warning(CODE_VERSION, "%s: the %s plug-in is interface version %s, newer than the %s this openRender implements: some features of the plug-in may be unused.\n", shown, family.interfaceName, describe(v.pluginVersion).c_str(), describe(hostForMajor).c_str());
        }
    }

    c.verdicts[key] = v;
    return v;
}

bool checkPluginTable(const SPluginFamily &family, const char *path, const OrTableHeader *table, size_t requiredSize, uint32_t versionFromSymbol) {
    const char *shown = path != nullptr ? path : "(unknown path)";
    if (table == nullptr) {
        error(CODE_VERSION, "%s: the %s plug-in returned no function table.\n", shown, family.interfaceName);
        return false;
    }

    // Eight bytes: the header. Nothing further is read before the size is known to cover it.
    const uint32_t structSize = table->structSize;
    const uint32_t abiVersion = table->abiVersion;

    if (structSize < sizeof(OrTableHeader) || structSize < requiredSize) {
        error(CODE_VERSION, "%s: the %s plug-in's function table is %u bytes, smaller than the %zu bytes this openRender needs (interface version %s): rebuild against the installed header.\n", shown, family.interfaceName, (unsigned)structSize, requiredSize, describe(versionFromSymbol).c_str());
        return false;
    }
    if (OR_ABI_MAJOR(abiVersion) != OR_ABI_MAJOR(versionFromSymbol)) {
        error(CODE_VERSION, "%s: the %s plug-in's function table is interface version %s (0x%08x) but its version symbol says %s (0x%08x): rebuild against the installed header.\n", shown, family.interfaceName, describe(abiVersion).c_str(), (unsigned)abiVersion, describe(versionFromSymbol).c_str(), (unsigned)versionFromSymbol);
        return false;
    }
    return true;
}
