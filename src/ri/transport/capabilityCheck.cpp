/**
 * Project: openRender
 *
 * File: capabilityCheck.cpp
 *
 * Description:
 *   transportCheckPair. See capabilityCheck.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "transport/capabilityCheck.h"

#include <cstring>

namespace {

    // The version `name` is provided at, or false when the set does not provide it.
    bool providedVersion(const OrCapability *provided, int numProvided, const char *name, uint32_t &version) {
        for (int i = 0; i < numProvided; ++i) {
            if (provided[i].name != nullptr && std::strcmp(provided[i].name, name) == 0) {
                version = provided[i].version;
                return true;
            }
        }
        return false;
    }

} // namespace

SPairVerdict transportCheckPair(const OrCapability *provided, int numProvided, const OrCapability *required, int numRequired, const OrCapability *optional, int numOptional) {
    SPairVerdict verdict;
    verdict.accepted = true;

    for (int i = 0; i < numRequired; ++i) {
        uint32_t have = 0;
        const bool found = providedVersion(provided, numProvided, required[i].name, have);
        if (!found || have < required[i].version) {
            verdict.accepted = false;
            verdict.shortfalls.push_back({required[i].name, required[i].version, found ? have : 0u, !found});
        }
    }

    for (int i = 0; i < numOptional; ++i) {
        uint32_t have = 0;
        if (!providedVersion(provided, numProvided, optional[i].name, have) || have < optional[i].version)
            verdict.absentOptional.push_back(optional[i].name);
    }

    return verdict;
}
