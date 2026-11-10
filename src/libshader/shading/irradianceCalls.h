/**
 * Project: openRender
 *
 * File: irradianceCalls.h
 *
 * Description:
 *   The cache step of occlusion() and indirectdiffuse() (spec 032, contracts/irradiance-cache.md), shared by the interpreter
 *   (IDEXPR) and the JIT (jitOcclusionBatch). The irradiance cache is passive storage in Texturing: this function asks it for
 *   a hit, hands a miss to the sampler the context's owner installed (IIrradianceSampler), and gives the sample it got back
 *   to the cache to keep. The point-based branch (a point hierarchy in place of the cache) draws nothing.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef IRRADIANCECALLS_H
#define IRRADIANCECALLS_H

class CShadingContext;
class CTexture3d;

namespace libshader {

    // Reads scratch.occlusionParams (pointbased, pointHierarchy, maxError, maxSolidAngle) of the shading state in progress.
    // Must be called after bindDraws() for the lane. `cache` is what getCache() answered: an irradiance cache, or nullptr
    // (a name registered as another kind of 3D file), which writes the "no joy" value {0, 0, 0, 1, 0, 0, 0} and draws nothing.
    // P, dPdu, dPdv and N are in the lookup's coordinate system; C receives 7 floats (irradiance, coverage, envdir).
    void irradianceLookup(CShadingContext &ctx, CTexture3d *cache, float *C, const float *P, const float *dPdu, const float *dPdv, const float *N);

} // namespace libshader

#endif
