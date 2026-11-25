/**
 * Project: openRender
 *
 * File: closureConsumer.h
 *
 * Description:
 *   The closure consumer (spec 035, contracts/consumer-and-backend.md): what the surface step hands the closure trees of a batch to. Spec 042's
 *   integrator installs one per frame; tests install one as the process default. The interface names no hider, integrator, light or
 *   renderer-core type (FR-021): ObjectHandle, SampleKey, RayType and BatchShape are Shading or Infra types.
 *
 *   consume() is called once per batch that built trees, on the thread that shaded it, before the batch's arena is released: everything in
 *   the record is valid only during the call (copy a tree with closureTreeCopy() to keep it). It may be called concurrently from several
 *   render threads, on different contexts; a consumer that aggregates synchronises itself. It must not call back into the context's shading
 *   entry points (shade, iterateLights, ...).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSURECONSUMER_H
#define CLOSURECONSUMER_H

#include "closureAbi.h"
#include "closureMode.h"
#include "common/sampling.h"
#include "libshader/shading/serviceTypes.h"
#include "libshader/shading/shaderGlobals.h"

class CShadingContext;

namespace openrender::shading {

    struct ClosureBatch {
            int numPoints;                              // numRealVertices of the batch
            const OrClosureNode *const *trees;          // numPoints roots; nullptr = empty tree
            const float *P, *N, *Ng, *dPdu, *dPdv, *I; // stride 3 floats, camera space
            const float *u, *v;                         // stride 1
            ObjectHandle object;                        // the shaded object (opaque, CShadingContext::objectHandle())
            const sampling::SampleKey *keys;            // the points' sample identities (spec 039)
            RayType rayType;                            // from the batch's ShaderGlobals
            int pathDepth;
            BatchShape shape;
            ClosureMode mode;                           // the effective mode (Both or Closures); Color never reaches here
            bool fromBxdf;                              // the trees came from a Bxdf binding (Phase 4)
    };

    class IClosureConsumer {
        public:
            virtual ~IClosureConsumer() = default;
            virtual void consume(CShadingContext &context, const ClosureBatch &batch) = 0;
    };

} // namespace openrender::shading

#endif
