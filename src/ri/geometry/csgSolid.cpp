/**
 * Project: openRender
 *
 * File: csgSolid.cpp
 *
 * Description:
 *   The renderable CSG solid: one placement or instance of a shared
 *   definition, resolved lazily on first use (spec 021).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "csgSolid.h"

// Spec 021 Phase 3 skeleton: declarations with inert bodies so the Phase 3
// tests compile and fail. Instance resolution arrives with T046.

CCSGInstanceState::CCSGInstanceState() : evaluator(NULL), passthrough(-1) {}

CCSGInstanceState::~CCSGInstanceState() {}

CCSGSolid::CCSGSolid(CAttributes *a, CXform *x, CCSGDefinition *d) : CObject(a, x), definition(d), state(NULL) {
    if (definition != NULL)
        definition->attach();
}

CCSGSolid::~CCSGSolid() {
    delete state.load();
    if (definition != NULL)
        definition->detach();
}

void CCSGSolid::intersect(CShadingContext *, CRay *) {}

void CCSGSolid::dice(CReyes *) {}

void CCSGSolid::instantiate(CAttributes *, CXform *, CRiInterface *) const {}

void CCSGSolid::resolve(CShadingContext *) {}
