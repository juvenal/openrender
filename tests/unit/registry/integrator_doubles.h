/**
 * Project: openRender
 *
 * File: integrator_doubles.h
 *
 * Description:
 *   Spec 030 (hider and integrator registry): the test double integrators of tests/unit/registry. They are
 *   defined and registered (OR_REGISTER_INTEGRATOR) in a test file, outside the renderer core, and log how the
 *   core called them. Lifecycle calls are single-threaded (contracts/integrator-interface.md "Threading"), so the
 *   logs need no lock.
 *
 *     double-needs-missing      requires a capability no hider provides ("test-unprovided" 1.0.0)
 *     double-needs-newer        requires "rasterization" 2.0.0 (the built-ins provide 1.0.0 or nothing)
 *     double-optional           requires nothing; lists "illumination-hook" and "rasterization" as optional, and
 *                               records which of the hider's provided capabilities its configure saw
 *     double-recorder           requires nothing; accepts every parameter except one named "rejected" and records
 *                               what configure received (type, numItems, converted data)
 *     double-configure-fails    configure returns a non-OK status
 *     double-prepare-fails      prepare returns a non-OK status
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef INTEGRATOR_DOUBLES_H
#define INTEGRATOR_DOUBLES_H

#include <string>
#include <vector>

enum EIntegratorDouble {
    DOUBLE_NEEDS_MISSING = 0,
    DOUBLE_NEEDS_NEWER,
    DOUBLE_OPTIONAL,
    DOUBLE_RECORDER,
    DOUBLE_CONFIGURE_FAILS,
    DOUBLE_PREPARE_FAILS,
    DOUBLE_COUNT
};

const char *integratorDoubleName(EIntegratorDouble kind);

// What configure received for one parameter, copied (the host's memory is valid during configure only).
struct SRecordedParameter {
        std::string name;
        int type = 0;
        int numItems = 0;
        std::vector<float> floats;       // TYPE_FLOAT: numItems; color/point/vector/normal: numItems*3; matrix: numItems*16
        std::vector<int> ints;           // TYPE_INTEGER: numItems
        std::vector<std::string> strings; // TYPE_STRING: numItems
};

struct SIntegratorLog {
        int created = 0;
        int destroyed = 0;
        int configured = 0;
        int prepared = 0;
        int finished = 0;
        std::string hiderName;                   // OrIntegratorSettings::hiderName at the last configure
        std::vector<std::string> hiderProvides;  // OrIntegratorSettings::hiderProvides at the last configure
        std::vector<SRecordedParameter> parameters; // the parameter list of the last configure
        void reset() { *this = SIntegratorLog(); }
        bool sawProvided(const char *capability) const {
            for (const std::string &p : hiderProvides)
                if (p == capability)
                    return true;
            return false;
        }
};

SIntegratorLog &integratorLog(EIntegratorDouble kind);
void resetIntegratorLogs();

#endif
