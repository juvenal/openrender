/**
 * Project: openRender
 *
 * File: situation.cpp
 *
 * Description:
 *   classifyShading(): the situation table of contracts/shader-globals.md as a pure function (spec 026).
 *   Each branch names the table row and the code location it reproduces at spec 026's starting tree.
 *   The eight oddities of research R13 are encoded as the table has them. They are not fixed here:
 *     1  camerahitmode applies after the shaders to a REYES grid (shading.cpp, the "primitive" overwrite of Oi)
 *     2  a raytrace camera hit reads specularhitmode, not camerahitmode (shading.cpp:915)
 *     3  an implicit-surface ray hit is shaded as SHADING_0D: no derivatives, no hit mode (O5 #119)
 *     4  a DSO-object ray hit calls shade() with vVertices = -1 (O2 #116)
 *     5  diffusehitmode is parsed and never read (rendererContext.cpp:3701-3704)
 *     6  the atmosphere runs only at depth 0 and never on a matte object (shading.cpp:1361, :933-936)
 *     7  a shaded transmission hit skips the displacement shader
 *     8  camerahitmode is ignored for 0-D points and by the raytrace camera hit
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "situation.h"

#include "shading.h" // EShadingDim

namespace libshader {

    using namespace openrender::shading;

    namespace {
        // 's' (shader) runs the surface; 'p' (primitive) reads the primitive's attributes instead
        HitMode modeOf(char c) { return c == 'p' ? HitMode::Primitive : HitMode::Shade; }
    } // namespace

    ShadingClass classifyShading(const ShadingSituation &s) {
        ShadingClass c{RayType::None, HitMode::Shade, TransportMode::Radiance, ShadingPass::Full};

        // Rows 8 and 9: the displacement passes run no shader of the surface, and read no hit mode.
        //   row 9: the displacement bound or grid-size estimate (displaceEstimate(), displaceOnly = 3):
        //          surface.cpp:304, :332, :334; geometryDispatch.cpp:455, :492
        //   row 8: the dicing and ray-tessellation passes (displace()): reyes.cpp:790, :870, :936, :1047, :1104;
        //          surface.cpp:1706-1794
        if (s.displaceOnly == 3) {
            c.pass = ShadingPass::DisplaceEstimate;
            return c;
        }
        if (s.displaceOnly != 0) {
            c.pass = ShadingPass::DisplaceOnly;
            return c;
        }

        // Row 10: photon emission (light shaders), photon.cpp:166-230: a phony object, SHADING_0D, the
        // illumination hook on. Row 11 (a photon hits a surface, photon.cpp:609-700) runs no shader and
        // is not a shading situation.
        if (s.illuminationHook) {
            c.rayType = RayType::Photon;
            c.transport = TransportMode::Importance;
            return c;
        }

        const bool zeroD = (s.dim == (int)SHADING_0D);

        // Row 5: a shaded shadow or transmission hit (traceTransmission sets inShadow, executeMisc.cpp:717).
        // transmissionhitmode (shading.cpp:889): "p" copies the attribute opacity and returns, "s" runs the
        // surface only (:907). A 0-D hit (row 6) has no hit mode.
        if (s.inShadow) {
            c.rayType = RayType::Shadow;
            c.hitMode = zeroD ? HitMode::Shade : modeOf(s.transmissionHitMode);
            return c;
        }

        if (s.rasterHider) {
            // Row 2: REYES points and curves, SHADING_0D (reyes.cpp:907, :968): camerahitmode is ignored (oddity 8)
            if (zeroD)
                return c;
            // Row 1: a REYES or zbuffer grid seen by the camera (reyes.cpp:1061, :1115). camerahitmode is
            // applied after the shaders (oddity 1): "p" overwrites Oi with the attribute opacity and keeps Ci
            // (shading.cpp:1374-1382).
            c.hitMode = modeOf(s.cameraHitMode);
            return c;
        }

        // A ray hit: rows 3, 4, 6 and 7 (and, with no hider at all, row 14: the RSLShading::shade() facade,
        // a bare execution, which has no ray and is told apart by its caller, not by this function).
        //   row 3: a raytrace camera hit (raytracer.cpp:652, depth 0) reads specularhitmode (shading.cpp:915),
        //          not camerahitmode (oddity 2)
        //   row 4: a secondary hit (depth >= 1, not in shadow) reads specularhitmode; diffusehitmode is
        //          never read (oddity 5)
        //   row 6: an implicit-surface hit is SHADING_0D (implicitSurface.cpp:258): no hit mode (oddity 3)
        //   row 7: a DSO-object hit (dlobject.cpp:176) is classified as rows 3 to 5 (oddity 4: vVertices = -1)
        c.rayType = (s.rayDepth == 0) ? RayType::Camera : RayType::Secondary;
        c.hitMode = zeroD ? HitMode::Shade : modeOf(s.specularHitMode);
        return c;
    }

} // namespace libshader
