/**
 * Project: openRender
 *
 * File: hiderIrradianceSampler.h
 *
 * Description:
 *   CIrradianceSampler: the Hiders' implementation of Shading's IIrradianceSampler (spec 032). Every hider installs the
 *   shared instance on the shading context it owns, and the renderer core installs it as the process-wide default.
 *
 *   The header is deliberately not named irradianceSampler.h: Shading owns a header of that name (the interface), and a
 *   same-named Hiders header would include itself, because a quoted include searches the including file's directory first.
 *   The bare name below therefore resolves to src/libshader/shading/.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDERIRRADIANCESAMPLER_H
#define HIDERIRRADIANCESAMPLER_H

#include "irradianceSampler.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceSampler
// Description			:	Computes a new irradiance sample by hemisphere sampling and ray tracing
// Comments				:	Stateless: any thread, each with its own context
class CIrradianceSampler final : public openrender::shading::IIrradianceSampler {
    public:
        void sample(CShadingContext &ctx, const float *cacheFrom, const float *P, const float *dPdu, const float *dPdv, const float *N, openrender::shading::IrradianceSample &out) override;
};

// The one shared instance (a function-local static)
openrender::shading::IIrradianceSampler &irradianceSamplerInstance();

#endif
