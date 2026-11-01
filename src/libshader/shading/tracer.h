/**
 * Project: openRender
 *
 * File: tracer.h
 *
 * Description:
 *   ITracer: the interface Shading uses to ask for rays (spec 026, contracts/tracer.md). Owned by
 *   Shading; implemented by the hiders (an adapter in 026, the hiders themselves from spec 029);
 *   reached through CRendererServices::tracer(). Installed once per frame and stateless: all per-thread
 *   state lives in the context argument.
 *
 *   Version: kRendererServicesVersion (rendererServices.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRACER_H
#define TRACER_H

#include "serviceTypes.h"

class CShadingContext;
class CRay;       // src/ri/core/ray.h (Infrastructure)
class CRayBundle; // shading.h

namespace openrender::shading {

    // Introduced in 2.0.0.
    class ITracer {
        public:
            virtual ~ITracer() = default;

            // Trace one ray and do not shade it.
            // In:  from, dir (unit length), t (maximum distance), tmin (bias), time, flags, da, db,
            //      key, depth (spec 039).
            // Out: object (null on a miss), u, v, N, t (the hit distance).
            // Thread-safe: called by every render thread, each with its own context.
            // Re-enters Shading: never.
            virtual void traceRay(CShadingContext &context, CRay *ray) = 0;

            // Trace a bundle one level deeper and shade what it hits: the contract of today's
            // CShadingContext::traceEx(). Past the maximum ray depth, every ray is reported through
            // bundle->postShade(numRays, rays) (a miss). Otherwise the rays are traced, and the bundle's
            // postTraceAction()/postShade()/post() protocol runs until the bundle has no rays left.
            // Thread-safe: as traceRay.
            // Re-enters Shading: yes. Hits are shaded through shade() on `context`, in a new shading
            // state one ray level deeper, and a shader there may trace again.
            virtual void traceBundle(CShadingContext &context, CRayBundle *bundle) = 0;
    };

} // namespace openrender::shading

#endif
