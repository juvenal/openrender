/**
 * Project: openRender
 *
 * File: situation.h
 *
 * Description:
 *   The derivation of a shading call's ray type, hit mode, transport and pass, as a pure function
 *   (spec 026, contracts/shader-globals.md, "Situation table"). It encodes the 14 rows of the table as the
 *   code at spec 026's starting tree has them, oddities included (research R13): a row that looks wrong is
 *   documented, not fixed.
 *
 *   Called only by tests (FR-018): nothing in the render path includes this header. Spec 029 makes
 *   shade() fill ShaderGlobals from the same derivation.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SITUATION_H
#define SITUATION_H

#include "shaderGlobals.h"

namespace libshader {

    // What a shading call knows about itself (shade()'s arguments and the context's state), plus the two
    // things only the frame knows: whether its hider rasterises, and the hit-mode attributes.
    struct ShadingSituation {
            int dim;                 // EShadingDim: SHADING_0D, SHADING_2D, SHADING_2D_GRID
            int displaceOnly;        // shade()'s argument: FALSE (0), TRUE, 3
            int inShadow;            // CShadingContext::inShadow
            int rayDepth;            // currentRayDepth
            bool illuminationHook;   // the photon pass: the illumination hook is on
            bool rasterHider;        // the frame's hider rasterises (reyes, zbuffer)
            char cameraHitMode;      // camerahitmode, 's' or 'p'
            char specularHitMode;    // specularhitmode
            char transmissionHitMode; // transmissionhitmode
    };

    struct ShadingClass {
            openrender::shading::RayType rayType;
            openrender::shading::HitMode hitMode;
            openrender::shading::TransportMode transport;
            openrender::shading::ShadingPass pass;
    };

    ShadingClass classifyShading(const ShadingSituation &s);

} // namespace libshader

#endif
