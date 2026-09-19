/**
 * Project: openRender
 *
 * File: compositor.cpp
 *
 * Description:
 *   Shared transparency/matte compositor (spec 008-hider-parity-convergence,
 *   R3). See compositor.h for the design rationale.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "compositor.h"
#include "renderer.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CCompositor
// Method				:	begin
// Description			:	Reset the running accumulator
// Return Value			:	-
// Comments				:
void CCompositor::begin(CompositeAccumulator &acc, float *extra) {
    acc.hasBase = false;
    acc.extra = extra;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCompositor
// Method				:	composite
// Description			:	Composite one sample (front-to-back) into the
//							running accumulator. Ported verbatim from
//							CStochastic::rasterEnd's compositeSampleLoop
//							(stochastic.cpp:660-796): the matte carve-out
//							always uses (1 +/- opacity) since matte samples
//							encode opacity as negative; non-matte samples use
//							the standard (1 - opacity) "over" remainder.
// Return Value			:	-
// Comments				:
void CCompositor::composite(CompositeAccumulator &acc, const CompositeSample &sample) {
    const int numExtraCompChannels = CRenderer::numExtraCompChannels;
    const int *compChannelOrder = CRenderer::compChannelOrder;
    const float *opacity = sample.opacity;
    const float *color = sample.color;
    const float *sampleExtra = sample.extraSamples;
    const bool isMatte = (opacity[0] < 0 || opacity[1] < 0 || opacity[2] < 0);

    if (!acc.hasBase) {
        // Base (nearest) sample
        if (isMatte) {
            initv(acc.color, 0);
            initv(acc.opacity, 0);
            acc.ropacity[0] = 1 + opacity[0];
            acc.ropacity[1] = 1 + opacity[1];
            acc.ropacity[2] = 1 + opacity[2];

            for (int es = 0; es < numExtraCompChannels; es++) {
                const int sampleOffset = compChannelOrder[es * 4];
                const int matteMode = compChannelOrder[es * 4 + 2];
                if (matteMode)
                    initv(acc.extra + sampleOffset, 0);
                else
                    movvv(acc.extra + sampleOffset, sampleExtra + sampleOffset);
            }
        } else {
            movvv(acc.color, color);
            movvv(acc.opacity, opacity);
            acc.ropacity[0] = 1 - opacity[0];
            acc.ropacity[1] = 1 - opacity[1];
            acc.ropacity[2] = 1 - opacity[2];

            for (int es = 0; es < numExtraCompChannels; es++) {
                const int sampleOffset = compChannelOrder[es * 4];
                movvv(acc.extra + sampleOffset, sampleExtra + sampleOffset);
            }
        }

        acc.hasBase = true;
        return;
    }

    // Continuation sample
    if (isMatte) {
        acc.ropacity[0] *= 1 + opacity[0];
        acc.ropacity[1] *= 1 + opacity[1];
        acc.ropacity[2] *= 1 + opacity[2];

        for (int es = 0; es < numExtraCompChannels; es++) {
            const int sampleOffset = compChannelOrder[es * 4];
            const int matteMode = compChannelOrder[es * 4 + 2];
            if (!matteMode)
                movvv(acc.extra + sampleOffset, sampleExtra + sampleOffset);
        }
    } else {
        acc.color[0] += acc.ropacity[0] * color[0];
        acc.color[1] += acc.ropacity[1] * color[1];
        acc.color[2] += acc.ropacity[2] * color[2];
        acc.opacity[0] += acc.ropacity[0] * opacity[0];
        acc.opacity[1] += acc.ropacity[1] * opacity[1];
        acc.opacity[2] += acc.ropacity[2] * opacity[2];

        for (int es = 0; es < numExtraCompChannels; es++) {
            const int sampleOffset = compChannelOrder[es * 4];
            acc.extra[sampleOffset + 0] += acc.ropacity[0] * sampleExtra[sampleOffset + 0];
            acc.extra[sampleOffset + 1] += acc.ropacity[1] * sampleExtra[sampleOffset + 1];
            acc.extra[sampleOffset + 2] += acc.ropacity[2] * sampleExtra[sampleOffset + 2];
        }

        acc.ropacity[0] *= 1 - opacity[0];
        acc.ropacity[1] *= 1 - opacity[1];
        acc.ropacity[2] *= 1 - opacity[2];
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCompositor
// Method				:	compositeNonComp
// Description			:	Test one candidate sample against the applicable
//							z-visibility threshold, latching its non-composited
//							AOVs on success. Ported verbatim from
//							stochastic.cpp:534-596, including the existing
//							checkMatteZThreshold asymmetry (applied whenever
//							the pixel contains any matte fragment, not only
//							when the tested sample itself is matte -- marked
//							FIXME in the source; preserved here rather than
//							"fixed", since parity with today's reyes output is
//							the correctness bar).
// Return Value			:	true if the sample passed its threshold and was
//							latched into `dst` (caller should record z and
//							stop searching); false if the caller should
//							advance to the next fragment.
// Comments				:
bool CCompositor::compositeNonComp(float *dst, const CompositeSample &sample,
                                    const vector zvisibilityThreshold, bool pixelHasMatte) {
    const int numExtraNonCompChannels = CRenderer::numExtraNonCompChannels;
    const int *nonCompChannelOrder = CRenderer::nonCompChannelOrder;
    const float *opacity = sample.opacity;
    const float *sampleExtra = sample.extraSamples;

    if (!pixelHasMatte) {
        const bool passesThreshold = (opacity[0] > zvisibilityThreshold[0]) ||
                                      (opacity[1] > zvisibilityThreshold[1]) ||
                                      (opacity[2] > zvisibilityThreshold[2]);

        if (!passesThreshold)
            return false;

        for (int es = 0; es < numExtraNonCompChannels; es++) {
            const int sampleOffset = nonCompChannelOrder[es * 4];
            const int numSamples = nonCompChannelOrder[es * 4 + 1];
            float *d = dst + sampleOffset;
            const float *s = sampleExtra + sampleOffset;
            for (int ess = numSamples; ess > 0; ess--)
                *d++ = *s++;
        }
        return true;
    }

    const bool passesMatteThreshold = (1 + opacity[0] > zvisibilityThreshold[0]) ||
                                       (1 + opacity[1] > zvisibilityThreshold[1]) ||
                                       (1 + opacity[2] > zvisibilityThreshold[2]);

    if (!passesMatteThreshold)
        return false;

    const bool isMatte = (opacity[0] < 0 || opacity[1] < 0 || opacity[2] < 0);

    for (int es = 0; es < numExtraNonCompChannels; es++) {
        const int sampleOffset = nonCompChannelOrder[es * 4];
        const int numSamples = nonCompChannelOrder[es * 4 + 1];
        const int matteMode = nonCompChannelOrder[es * 4 + 2];
        float *d = dst + sampleOffset;
        const float *s = (isMatte && matteMode) ? (CRenderer::sampleDefaults + sampleOffset) : (sampleExtra + sampleOffset);
        for (int ess = numSamples; ess > 0; ess--)
            *d++ = *s++;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCompositor
// Method				:	evaluateDepth
// Description			:	Resolve one sample point's (one reyes subsample's
//							fragment list, or one raytrace ray's hit chain)
//							depth-filtered z, honoring the zvisibilityThreshold
//							exclusion. Ported verbatim from
//							CStochastic::rasterEnd's per-subsample Z[0]/Z2[0]
//							resolution (stochastic.cpp:521-624): walk the
//							front-to-back candidates, matte-aware threshold
//							test identical to compositeNonComp's, stop at the
//							first passing candidate. Mid mode additionally
//							searches past it for a second passing candidate
//							(falling back to the first if none), floors it
//							against `zold`, then returns the midpoint --
//							exactly reyes's existing (documented-buggy)
//							behavior, preserved rather than "fixed" (parity
//							with today's reyes output is the correctness bar).
//							Each z is clip-corrected against
//							CRenderer::clipMax independently, matching reyes's
//							own per-component clip-correct step.
// Return Value			:	the resolved z for this sample point.
// Comments				:	Two different threshold-formula policies, both
//							ported verbatim from the original reyes code: the
//							first-candidate search uses `pixelHasMatte`,
//							exactly mirroring compositeNonComp; the Mid-mode
//							second-candidate search decides per-candidate
//							instead. This asymmetry is a preserved quirk, not
//							an oversight -- see the header comment.
static bool passesZThresholdPixelWide(const float *opacity, const vector zvisibilityThreshold, bool pixelHasMatte) {
    if (pixelHasMatte) {
        return (1 + opacity[0] > zvisibilityThreshold[0]) ||
               (1 + opacity[1] > zvisibilityThreshold[1]) ||
               (1 + opacity[2] > zvisibilityThreshold[2]);
    }
    return (opacity[0] > zvisibilityThreshold[0]) ||
           (opacity[1] > zvisibilityThreshold[1]) ||
           (opacity[2] > zvisibilityThreshold[2]);
}

static bool passesZThresholdPerCandidate(const float *opacity, const vector zvisibilityThreshold) {
    const bool isMatte = (opacity[0] < 0 || opacity[1] < 0 || opacity[2] < 0);
    if (isMatte) {
        return (1 + opacity[0] > zvisibilityThreshold[0]) ||
               (1 + opacity[1] > zvisibilityThreshold[1]) ||
               (1 + opacity[2] > zvisibilityThreshold[2]);
    }
    return (opacity[0] > zvisibilityThreshold[0]) ||
           (opacity[1] > zvisibilityThreshold[1]) ||
           (opacity[2] > zvisibilityThreshold[2]);
}

float CCompositor::evaluateDepth(const float *zs, const float *opacities, int count,
                                  DepthFilterMode mode, const vector zvisibilityThreshold,
                                  bool pixelHasMatte, float zold) {
    int i = 0;
    float z0 = C_INFINITY;

    for (; i < count; i++) {
        if (passesZThresholdPixelWide(opacities + i * 3, zvisibilityThreshold, pixelHasMatte)) {
            z0 = zs[i];
            break;
        }
    }

    if (z0 >= CRenderer::clipMax)
        z0 = C_INFINITY;

    if (mode != DepthFilterMode::Mid || i == count)
        return z0;

    float z2 = z0; // fallback: no second candidate, use the first
    for (int j = i + 1; j < count; j++) {
        if (passesZThresholdPerCandidate(opacities + j * 3, zvisibilityThreshold)) {
            z2 = zs[j];
            break;
        }
    }

    if (zold > z2)
        z2 = zold;

    if (z2 >= CRenderer::clipMax)
        z2 = C_INFINITY;

    return 0.5f * (z0 + z2);
}
