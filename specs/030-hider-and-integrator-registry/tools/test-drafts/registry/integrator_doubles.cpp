/**
 * Project: openRender
 *
 * File: integrator_doubles.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry): the test double integrators (see integrator_doubles.h). Linked
 *   into the test executables that need an integrator from outside the core; nothing in src/ names it.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "integrator_doubles.h"

#include <cstring>

#include "common/rendererc.h"
#include "transport/capability.h"
#include "transport/integratorAbi.h"
#include "transport/integratorAbi.hpp"

namespace {

    const char *const kNames[DOUBLE_COUNT] = {"double-needs-missing", "double-needs-newer", "double-optional",
                                               "double-recorder",      "double-configure-fails", "double-prepare-fails"};

    SIntegratorLog gLogs[DOUBLE_COUNT];

    // An instance only remembers which double it is.
    struct SInstance {
            EIntegratorDouble kind;
    };

    SInstance *self(OrIntegratorInstance *i) {
        return reinterpret_cast<SInstance *>(i);
    }

    template <int K>
    OrIntegratorInstance *createK(void) {
        ++gLogs[K].created;
        return reinterpret_cast<OrIntegratorInstance *>(new SInstance{static_cast<EIntegratorDouble>(K)});
    }

    void destroyInstance(OrIntegratorInstance *instance) {
        ++gLogs[self(instance)->kind].destroyed;
        delete self(instance);
    }

    void record(SIntegratorLog &log, const OrIntegratorSettings *settings) {
        log.hiderName = settings->hiderName != nullptr ? settings->hiderName : "";
        log.hiderProvides.clear();
        for (int i = 0; i < settings->numHiderProvides; ++i)
            log.hiderProvides.push_back(settings->hiderProvides[i].name);
        log.parameters.clear();
        for (int i = 0; i < settings->numParameters; ++i) {
            const OrIntegratorParameter &p = settings->parameters[i];
            SRecordedParameter r;
            r.name = p.name;
            r.type = p.type;
            r.numItems = p.numItems;
            int perItem = 0; // floats per item
            switch (p.type) {
            case TYPE_FLOAT:
                perItem = 1;
                break;
            case TYPE_COLOR:
            case TYPE_POINT:
            case TYPE_VECTOR:
            case TYPE_NORMAL:
                perItem = 3;
                break;
            case TYPE_MATRIX:
                perItem = 16;
                break;
            default:
                break;
            }
            if (perItem > 0) {
                const float *f = static_cast<const float *>(p.data);
                r.floats.assign(f, f + p.numItems * perItem);
            }
            else if (p.type == TYPE_INTEGER) {
                const int *v = static_cast<const int *>(p.data);
                r.ints.assign(v, v + p.numItems);
            }
            else if (p.type == TYPE_STRING) {
                const char *const *s = static_cast<const char *const *>(p.data);
                for (int k = 0; k < p.numItems; ++k)
                    r.strings.push_back(s[k]);
            }
            log.parameters.push_back(r);
        }
    }

    OrStatus configureDouble(OrIntegratorInstance *instance, const OrIntegratorSettings *settings, int *accepted) {
        const EIntegratorDouble kind = self(instance)->kind;
        SIntegratorLog &log = gLogs[kind];
        ++log.configured;
        record(log, settings);
        for (int i = 0; i < settings->numParameters; ++i)
            accepted[i] = (kind == DOUBLE_RECORDER && std::strcmp(settings->parameters[i].name, "rejected") != 0) ? 1 : 0;
        return kind == DOUBLE_CONFIGURE_FAILS ? OR_STATUS_ERROR : OR_STATUS_OK;
    }

    OrStatus prepareDouble(OrIntegratorInstance *instance, const OrTransportHost *, const OrIntegratorFrame *) {
        const EIntegratorDouble kind = self(instance)->kind;
        ++gLogs[kind].prepared;
        return kind == DOUBLE_PREPARE_FAILS ? OR_STATUS_ERROR : OR_STATUS_OK;
    }

    void finishDouble(OrIntegratorInstance *instance, const OrTransportHost *) {
        ++gLogs[self(instance)->kind].finished;
    }

    const OrCapability kNeedsMissing[] = {{"test-unprovided", OR_ABI_VERSION(1, 0, 0)}};
    const OrCapability kNeedsNewer[] = {{OR_CAP_RASTERIZATION, OR_ABI_VERSION(2, 0, 0)}};
    const OrCapability kOptionalHook[] = {{OR_CAP_ILLUMINATION_HOOK, OR_ABI_VERSION(1, 0, 0)}, {OR_CAP_RASTERIZATION, OR_ABI_VERSION(1, 0, 0)}};

    template <int K>
    const OrIntegratorTable *tableK() {
        static const OrIntegratorTable table = {
            {sizeof(OrIntegratorTable), OR_INTEGRATOR_ABI_VERSION},
            kNames[K],
            nullptr,
            0,
            K == DOUBLE_NEEDS_MISSING ? kNeedsMissing : (K == DOUBLE_NEEDS_NEWER ? kNeedsNewer : nullptr),
            (K == DOUBLE_NEEDS_MISSING || K == DOUBLE_NEEDS_NEWER) ? 1 : 0,
            K == DOUBLE_OPTIONAL ? kOptionalHook : nullptr,
            K == DOUBLE_OPTIONAL ? 2 : 0,
            createK<K>,
            destroyInstance,
            configureDouble,
            prepareDouble,
            finishDouble};
        return &table;
    }

} // namespace

const char *integratorDoubleName(EIntegratorDouble kind) {
    return kNames[kind];
}

SIntegratorLog &integratorLog(EIntegratorDouble kind) {
    return gLogs[kind];
}

void resetIntegratorLogs() {
    for (SIntegratorLog &l : gLogs)
        l.reset();
}

// Plain getters: the registration macro takes a function name.
const OrIntegratorTable *tableNeedsMissing() {
    return tableK<DOUBLE_NEEDS_MISSING>();
}
const OrIntegratorTable *tableNeedsNewer() {
    return tableK<DOUBLE_NEEDS_NEWER>();
}
const OrIntegratorTable *tableOptional() {
    return tableK<DOUBLE_OPTIONAL>();
}
const OrIntegratorTable *tableRecorder() {
    return tableK<DOUBLE_RECORDER>();
}
const OrIntegratorTable *tableConfigureFails() {
    return tableK<DOUBLE_CONFIGURE_FAILS>();
}
const OrIntegratorTable *tablePrepareFails() {
    return tableK<DOUBLE_PREPARE_FAILS>();
}

OR_REGISTER_INTEGRATOR(tableNeedsMissing)
OR_REGISTER_INTEGRATOR(tableNeedsNewer)
OR_REGISTER_INTEGRATOR(tableOptional)
OR_REGISTER_INTEGRATOR(tableRecorder)
OR_REGISTER_INTEGRATOR(tableConfigureFails)
OR_REGISTER_INTEGRATOR(tablePrepareFails)
