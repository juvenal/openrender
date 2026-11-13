/**
 * Project: openRender
 *
 * File: classicIntegrator.h
 *
 * Description:
 *   The `classic` integrator (spec 030, FR-012, contracts/integrator-interface.md "classic"): the transport every frame used
 *   before integrators existed. It requires and uses no capability, accepts no parameter and does nothing in any lifecycle
 *   slot, so a frame that selects it renders exactly as before. Each lifecycle slot counts its calls
 *   (transportClassicLifecycleCounts), which tests read; the counters change no output.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRANSPORT_CLASSICINTEGRATOR_H
#define TRANSPORT_CLASSICINTEGRATOR_H

#include <cstdint>

#include "transport/integratorAbi.h"

const OrIntegratorTable *orIntegratorTableClassic();

struct SClassicLifecycleCounts {
        uint64_t configure;
        uint64_t prepare;
        uint64_t finish;
};

// A snapshot of the three counters. Used only by tests.
SClassicLifecycleCounts transportClassicLifecycleCounts();

#endif
