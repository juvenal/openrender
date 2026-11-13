/**
 * Project: openRender
 *
 * File: capability.h
 *
 * Description:
 *   The capability vocabulary (spec 030, FR-011, contracts/integrator-interface.md). Transport domain; strict C17 and C++20,
 *   includes only orplugin.h.
 *
 *   A hider PROVIDES a set of capabilities; an integrator REQUIRES a set and may OPTIONALLY use another. A requirement is
 *   met when the same name is provided with provided >= required (the OR_ABI_VERSION encoding compares as an integer).
 *   Absence means the hider is limited in that respect. The vocabulary is versioned with the integrator interface: the
 *   version-history table in integratorAbi.h is its record. Names are exact and case-sensitive.
 *
 *   Reserved, added by the spec that needs one (a MINOR step of the integrator interface): ray-intersection,
 *   ray-occlusion, light-sampling, light-emission-sampling, surface-closures, volume-closures, reverse-pdf,
 *   derivatives, path-connections, batched-execution, spectral-transport, deep-output.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRANSPORT_CAPABILITY_H
#define TRANSPORT_CAPABILITY_H

#include "orplugin.h"

typedef struct OrCapability {
        const char *name; /* a name below; exact, case-sensitive */
        uint32_t version; /* OR_ABI_VERSION encoding; a requirement is met when provided >= required */
} OrCapability;

#define OR_CAP_RASTERIZATION "rasterization"           /* the hider rasterizes submitted objects        */
#define OR_CAP_DISPLAY_OUTPUT "display-output"         /* the hider produces display output             */
#define OR_CAP_DISPLAY_CHANNELS "display-channels"     /* arbitrary display channels (AOVs)             */
#define OR_CAP_DEEP_SHADOW_OUTPUT "deep-shadow-output" /* deep shadow (TSM) output                     */
#define OR_CAP_ILLUMINATION_HOOK "illumination-hook"   /* lights driven through the illumination hook   */

/* Every capability of this version of the vocabulary is at 1.0.0. */
#define OR_CAP_VERSION OR_ABI_VERSION(1, 0, 0)

#endif
