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

    // Introduced in 2.0.0. Implemented by the hiders (CHider): a hider is the tracer of the shading context it owns and installs
    // itself with CShadingContext::setTracer(). Shading asks the context's own tracer first and CRendererServices::tracer()
    // (rendererServices.h) only for a context with none. With no tracer Shading warns once per kind and treats every ray as a miss.
    //
    // The fields of CRay are in src/ri/core/ray.h and those of CRayBundle in shading.h (class CRayBundle).
    //
    // Space and units: every vector is in camera space (the "current" system); distances are camera-space
    // units; `time` is the normalised shutter time, 0 <= time <= 1.
    class ITracer {
        public:
            virtual ~ITracer() = default;

            // Trace one ray and do not shade it.
            // In (set by the caller):
            //   from, dir      origin and direction; dir must be of unit length;
            //   t              the maximum distance to look for a hit; tmin the bias (the ray starts at tmin);
            //   time           the ray's time (0..1);
            //   flags          the visibility mask of the ray: ATTRIBUTES_FLAGS_*_VISIBLE bits; a primitive
            //                  is a candidate only if its attributes carry one of them (primary, photon,
            //                  specular, diffuse, transmission);
            //   da, db         the linear ray differential: the ray's footprint at distance t is da * t + db;
            //   key, depth     the ray's sample identity (spec 039) and its depth, 0 for a primary ray;
            //   cameraImportance  true if jimp already holds the camera importance (trace() then keeps it).
            // Out:
            //   object   the intersected surface, or nullptr for a miss. Shading may dereference a non-null
            //            object (to read its attributes and to shade it), so it must point at a live surface
            //            for the rest of the call;
            //   u, v     the parametric coordinates of the hit;
            //   N        the surface normal at the hit, camera space;
            //   t        overwritten with the hit distance on a hit; unchanged on a miss;
            //   jimp     written (the ray's jittered importance, drawn from key and depth) unless
            //            cameraImportance is true, in which case it is kept;
            //   invDir   written: 1 / dir, component-wise, in double precision.
            //            jimp and invDir are working data of the hiders: only the hiders read them, Shading
            //            never does, so a tracer that does not use them may leave them unwritten.
            // A miss leaves u, v and N unspecified, and sets object to nullptr.
            // Thread-safety: called by every render thread, each with its own context.
            // Re-enters Shading: never.
            virtual void traceRay(CShadingContext &context, CRay *ray) = 0;

            // Trace a bundle one level deeper and shade what it hits: the contract of today's
            // CShadingContext::traceEx(). The bundle (shading.h) holds numRays, rays (an array of CRay *),
            // a label, a transparency depth, and the protocol below. The ray inputs are those of traceRay.
            //
            // The depth rule and the new shading state live in the context: the current ray depth
            // (currentRayDepth) and the list of free shading states (freeStates) are private to
            // CShadingContext, while newState() is public. An implementer outside the context therefore gets
            // the depth rule and the state recycling only by calling context.enterRayLevel() and leaveRayLevel(),
            // the one public way to apply them (the hiders do exactly that); one that does not call them must
            // keep and apply its own depth count, and cannot read or change the context's.
            // The maximum ray depth is the Option "trace" "maxdepth" (default 5), compared with the
            // context's current ray depth. At the maximum (no deeper level is allowed), nothing is traced:
            // the bundle gets ONE call bundle->postShade(numRays, rays), the miss form (without a varying
            // array), for all its rays, and no postTraceAction() or post() call.
            // Below the maximum:
            //   1. every ray is traced (the renderer's clip planes shorten ray->t), as traceRay does;
            //   2. bundle->postTraceAction() is called; it returns FALSE to stop (the pass ends, nothing is
            //      shaded or reported, and the call returns) and TRUE to go on;
            //   3. the rays are split by what they hit. Hits (ray->object != nullptr) are grouped per
            //      object, shaded in chunks of at most maxGridSize() rays in a new shading state one ray
            //      level deeper, and for each chunk bundle->postShade(n, rays, varying) is called: `rays` are
            //      the n rays of the chunk, and `varying` is that state's array of per-variable buffers,
            //      indexed by VARIABLE_*, where the result of ray k is at varying[VARIABLE_CI] + 3 * k
            //      (colour, 3 floats) and varying[VARIABLE_OI] + 3 * k (opacity, 3 floats), and likewise for
            //      any other variable the bundle asked for. A tracer that builds its own varying array (a
            //      double) learns which variables to fill from its bundle: CTraceBundle reads Ci and Oi,
            //      CTransmissionBundle reads Oi, and CGatherBundle reads the variables named by the shader's
            //      gather outputs. The array has one buffer per global variable
            //      (CRendererServices::numGlobalVariables(), indexed by VARIABLE_*), each at least
            //      numFloats(variable) * n floats for a varying variable (n <= maxGridSize()) and numFloats
            //      for a uniform or constant one, both from CRendererServices::globalVariable(i). Misses
            //      (object == nullptr) are reported with bundle->postShade(n, rays), without a varying array;
            //   4. bundle->post() is called;
            //   5. numRays and rays are RE-READ from the bundle: when numRays is 0 the call returns;
            //      otherwise every remaining ray is continued (tmin = t + a small epsilon, t = infinity)
            //      and traced, and the sequence repeats from step 2. The bundle rewrites its rays array in
            //      place between passes.
            // Thread-safety: as traceRay.
            // Re-enters Shading: yes. Hits are shaded through shade() on `context`, in a new shading
            // state one ray level deeper, and a shader there may trace again. postShade() may also run
            // the segment's volume shader on `context` (spec 056), in a further state at the segment's depth.
            virtual void traceBundle(CShadingContext &context, CRayBundle *bundle) = 0;
    };

} // namespace openrender::shading

#endif
