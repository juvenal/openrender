/**
 * Project: openRender
 *
 * File: hider.h
 *
 * Description:
 *   CHider, the base of every hider (spec 029, contracts/hider.md). A hider owns the shading context of its
 *   render thread instead of being one: it runs the thread's rendering loop, and the renderer core creates and
 *   destroys it together with its context. Phase 5 form: the ray tracing and the shading production join the
 *   hider in later phases of the spec.
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
#include "shading.h"                              // CShadingContext (Shading; Hiders -> Shading is allowed)

static_assert(openrender::shading::kRendererServicesVersion.major == 2, "CHider is written against services version 2");

///////////////////////////////////////////////////////////////////////
// Class				:	CHider
// Description			:	The base of every hider. It owns the shading context of its render thread.
// Comments				:	One hider per render thread, created and destroyed by the renderer core with its context.
class CHider {
    public:
        explicit CHider(int thread);
        virtual ~CHider(); // the context is destroyed after the derived hider's destructor body

        // Render this thread's share of the frame (moved from CShadingContext)
        virtual void renderingLoop() = 0;

        // Inline: no link symbol (Geometry calls it)
        CShadingContext &shadingContext() { return context; }
        const CShadingContext &shadingContext() const { return context; }

    protected:
        CShadingContext context; // declared first: built before, destroyed after, the rest of the hider
};

#endif
