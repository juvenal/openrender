/**
 * Project: openRender
 *
 * File: hiderTracer.h
 *
 * Description:
 *   CHiderTracer: the hiders' answer to Shading's ITracer (spec 026, contracts/tracer.md). An adapter over
 *   CShadingContext::trace() and traceEx(), which stay where they are (trace.cpp). Stateless: all trace state is a
 *   member of the context passed in.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDERTRACER_H
#define HIDERTRACER_H

#include "libshader/shading/rendererServices.h"
#include "libshader/shading/tracer.h"

static_assert(openrender::shading::kRendererServicesVersion.major == 2, "CHiderTracer is written against services version 2");

class CHiderTracer final : public openrender::shading::ITracer {
    public:
        void traceRay(CShadingContext &context, CRay *ray) override;
        void traceBundle(CShadingContext &context, CRayBundle *bundle) override;
};

#endif
