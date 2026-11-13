/**
 * Project: openRender
 *
 * File: classicIntegrator.cpp
 *
 * Description:
 *   The `classic` integrator. See classicIntegrator.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "transport/classicIntegrator.h"

#include <atomic>

namespace {

    std::atomic<uint64_t> configureCalls{0};
    std::atomic<uint64_t> prepareCalls{0};
    std::atomic<uint64_t> finishCalls{0};

    // `classic` keeps no state: create hands out the address of a static, which destroy ignores.
    char classicInstance;

    OrIntegratorInstance *classicCreate(void) {
        return reinterpret_cast<OrIntegratorInstance *>(&classicInstance);
    }

    void classicDestroy(OrIntegratorInstance *) {}

    OrStatus classicConfigure(OrIntegratorInstance *, const OrIntegratorSettings *settings, int *acceptedParameters) {
        configureCalls.fetch_add(1, std::memory_order_relaxed);
        // `classic` defines no parameter: every one of the statement's parameters is reported by the host.
        if (settings != nullptr && acceptedParameters != nullptr)
            for (int i = 0; i < settings->numParameters; ++i)
                acceptedParameters[i] = 0;
        return OR_STATUS_OK;
    }

    OrStatus classicPrepare(OrIntegratorInstance *, const OrTransportHost *, const OrIntegratorFrame *) {
        prepareCalls.fetch_add(1, std::memory_order_relaxed);
        return OR_STATUS_OK;
    }

    void classicFinish(OrIntegratorInstance *, const OrTransportHost *) {
        finishCalls.fetch_add(1, std::memory_order_relaxed);
    }

    const OrIntegratorTable kClassic = {
        {sizeof(OrIntegratorTable), OR_INTEGRATOR_ABI_VERSION},
        "classic",
        nullptr,
        0, // no aliases
        nullptr,
        0, // requires nothing
        nullptr,
        0, // uses nothing
        classicCreate,
        classicDestroy,
        classicConfigure,
        classicPrepare,
        classicFinish,
    };

} // namespace

const OrIntegratorTable *orIntegratorTableClassic() {
    return &kClassic;
}

SClassicLifecycleCounts transportClassicLifecycleCounts() {
    return {configureCalls.load(std::memory_order_relaxed), prepareCalls.load(std::memory_order_relaxed), finishCalls.load(std::memory_order_relaxed)};
}
