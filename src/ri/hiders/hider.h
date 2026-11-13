/**
 * Project: openRender
 *
 * File: hider.h
 *
 * Description:
 *   CHider, the base of every hider (spec 029, contracts/hider.md). A hider owns the shading context of its
 *   render thread instead of being one: it runs the thread's rendering loop, and the renderer core creates and
 *   destroys it together with its context. It is also the tracer of its context (ITracer, spec 029 Phase 6); the
 *   shading production joins it in a later phase of the spec.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDER_H
#define HIDER_H

#include "libshader/shading/rendererServices.h" // kRendererServicesVersion
#include "libshader/shading/shadingProducer.h"    // IShadingProducer
#include "libshader/shading/tracer.h"           // ITracer
#include "shading.h"                              // CShadingContext (Shading; Hiders -> Shading is allowed)

static_assert(openrender::shading::kRendererServicesVersion.major == 2, "CHider is written against services version 2");

class CObject;

///////////////////////////////////////////////////////////////////////
// Class				:	CHider
// Description			:	The base of every hider. It owns the shading context of its render thread.
// Comments				:	One hider per render thread, created and destroyed by the renderer core with its context.
class CHider : public openrender::shading::ITracer, public openrender::shading::IShadingProducer {
    public:
        explicit CHider(int thread); // constructs the context and installs itself as its tracer and its producer
        virtual ~CHider(); // the context is destroyed after the derived hider's destructor body

        // Render this thread's share of the frame (moved from CShadingContext)
        virtual void renderingLoop() = 0;

        // Draw an object into this hider's frame. The default does nothing; the hiders that rasterize override it (spec 030: the
        // core stops naming a concrete hider type to submit objects).
        virtual void drawObject(CObject *) {}

        // Inline: no link symbol (Geometry calls it)
        CShadingContext &shadingContext() { return context; }
        const CShadingContext &shadingContext() const { return context; }

        // ITracer (tracer.h; signatures unchanged). `ctx` is always this hider's own context (asserted in debug). The bodies
        // are in trace.cpp; every caller goes through this signature, there is no one-argument traceRay().
        void traceRay(CShadingContext &ctx, CRay *ray) override;
        void traceBundle(CShadingContext &ctx, CRayBundle *bundle) override;

        // IShadingProducer (shadingProducer.h): the target of the context's Geometry-facing shade(CSurface *, ...). Builds the point
        // source and the batch description and calls context.shade(ShaderGlobals, IPointSource) (shadingProducer.cpp)
        void shadeSurface(CShadingContext &ctx, CSurface *surface, int uVertices, int vVertices, EShadingDim dim, unsigned int usedParameters,
                          int displaceOnly) override;

    protected:
        // True for the hiders that rasterize (the frame's hider kind, which the batch's situation depends on)
        virtual bool rasterizes() const { return false; }

        // Trace a bundle's rays, group them by object, shade, postShade/post, continue (the former CShadingContext::trace(CRayBundle *))
        void traceGroups(CRayBundle *bundle);

        CShadingContext context; // declared first: built before, destroyed after, the rest of the hider
};

#endif
