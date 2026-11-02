/**
 * Project: openRender
 *
 * File: hiderTracer.cpp
 *
 * Description:
 *   See hiderTracer.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "hiderTracer.h"

#include "shading.h"

void CHiderTracer::traceRay(CShadingContext &context, CRay *ray) {
    context.trace(ray);
}

void CHiderTracer::traceBundle(CShadingContext &context, CRayBundle *bundle) {
    context.traceEx(bundle);
}
