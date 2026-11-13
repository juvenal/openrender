/**
 * Project: openRender
 *
 * File: capabilityCheck.h
 *
 * Description:
 *   The pair check (spec 030, FR-013, contracts/integrator-interface.md): a pure function over a hider's provided
 *   capabilities and an integrator's required and optional ones. It reports nothing; the caller writes the diagnostic.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRANSPORT_CAPABILITYCHECK_H
#define TRANSPORT_CAPABILITYCHECK_H

#include <cstdint>
#include <vector>

#include "transport/capability.h"

// A requirement that is not met. `missing` is true when the capability is not provided at all (`provided` is then 0);
// false when it is provided at a lower version than `required`.
struct SPairShortfall {
        const char *name;
        uint32_t required;
        uint32_t provided;
        bool missing;
};

struct SPairVerdict {
        bool accepted;                            // false when any required capability is missing or too old
        std::vector<SPairShortfall> shortfalls;   // in the order of the required array
        std::vector<const char *> absentOptional; // optional capabilities absent or older; they never refuse the pair
};

SPairVerdict transportCheckPair(const OrCapability *provided, int numProvided, const OrCapability *required, int numRequired, const OrCapability *optional, int numOptional);

#endif
