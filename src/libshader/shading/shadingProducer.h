/**
 * Project: openRender
 *
 * File: shadingProducer.h
 *
 * Description:
 *   IShadingProducer: the target of CShadingContext's Geometry-facing forwarding entry, shade(CSurface *, ...)
 *   (spec 029, contracts/point-source-and-globals.md). Implemented by the hider that owns the context (CHider) and
 *   installed with CShadingContext::setProducer(). It builds the point source and the batch description and calls
 *   CShadingContext::shade(const ShaderGlobals &, IPointSource &).
 *
 *   Introduced in 2.1.0. Version: kRendererServicesVersion (rendererServices.h).
 *   Re-enters Shading: yes, exactly once per call.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADINGPRODUCER_H
#define SHADINGPRODUCER_H

#include "shading.h" // CShadingContext, EShadingDim

namespace openrender::shading {

    class IShadingProducer {
        public:
            virtual ~IShadingProducer() = default; // inline: the vtable and the typeinfo are weak, wherever they are used

            // `surface` is opaque to Shading; the producer is the one that knows what it is.
            virtual void shadeSurface(CShadingContext &context, CSurface *surface, int uVertices, int vVertices, EShadingDim dim,
                                      unsigned int usedParameters, int displaceOnly) = 0;
    };

} // namespace openrender::shading

#endif
