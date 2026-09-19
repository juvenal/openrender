/**
 * Project: openRender
 *
 * File: pixelFilter.h
 *
 * Description:
 *   Shared pixel-filter accumulate/normalize step (spec 008-hider-parity-
 *   convergence, R4). Consolidates the precomputed-kernel weight lookup and
 *   the weighted per-channel splat/gather arithmetic that CStochastic,
 *   CRaytracer, and CZbuffer each wrote out by hand, in the same shape, at
 *   their own rasterEnd/splatSamples call sites. Ported verbatim from those
 *   call sites -- ordering and arithmetic are unchanged, only moved -- so
 *   introducing this header does not alter rendered output (FR-021).
 *
 *   Deliberately NOT the CColor/(sampleX,sampleY) object-style interface
 *   sketched in contracts/filter-module-contract.md: none of the three
 *   hiders' sample buffers are laid out that way (stochastic and zbuffer
 *   read/write fixed-offset slices of a flat float array with a per-hider
 *   channel order; raytracer reads/writes a uniform CRenderer::numSamples
 *   stride). The contract itself flags that illustrative shape as TBD at
 *   implementation time. Channel-order marshalling (which src offset feeds
 *   which dest offset, and any derived/computed channel such as zbuffer's
 *   coverage test) stays at each call site; only the generic weighted
 *   multiply-add and the normalize-by-accumulated-weight loop are shared.
 *
 *   Each hider's existing normalize policy is preserved exactly, as a
 *   caller-side choice rather than something this module decides:
 *     - CZbuffer: never normalizes (single precomputed-kernel mode only).
 *     - CStochastic: normalizes only in continuous filter mode; precomputed
 *       mode does not call normalizeByWeight at all.
 *     - CRaytracer: always normalizes, in both filter modes, once per bucket
 *       after all samples have been splatted.
 *   Collapsing these onto one unconditional normalize would divide by a
 *   value some callers currently never compute, changing output at the
 *   bit level and violating FR-021.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef PIXELFILTER_H
#define PIXELFILTER_H

#include "renderer.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPixelFilterAccumulator
// Description			:	Static helpers around the shared, already-computed
//							CRenderer::pixelFilterKernel / CRenderer::pixelFilter():
//							kernel-weight lookup, weighted channel accumulate,
//							and post-accumulate normalize-by-weight.
class CPixelFilterAccumulator {
public:
    // Precomputed-kernel weight for a sample at grid position (sx, sy) within
    // a filterWidth-wide kernel footprint. Identical to the
    // CRenderer::pixelFilterKernel[sy * filterWidth + sx] lookup every
    // precomputed-mode caller (CStochastic, CZbuffer, CRaytracer) already
    // performed inline.
    static inline float precomputedWeight(int sx, int sy, int filterWidth) {
        return CRenderer::pixelFilterKernel[sy * filterWidth + sx];
    }

    // Accumulate dest[k] += weight * src[k] for k in [0, numChannels).
    // Channel-order marshalling (which src offset maps to which dest offset,
    // and any derived channel that isn't a plain weighted copy) is the
    // caller's responsibility -- this is the inner multiply-add loop every
    // hider's filter step already performed by hand.
    static inline void splat(float *dest, const float *src, int numChannels, float weight) {
        for (int k = 0; k < numChannels; k++) {
            dest[k] += weight * src[k];
        }
    }

    // Normalize numPixels pixels (each numChannels wide, contiguous) by their
    // accumulated filter weight, for pixels whose weight is > 0. Only call
    // this where the caller's existing behavior already normalizes -- see
    // the file header for which hiders/modes that is.
    static inline void normalizeByWeight(float *pixels, const float *weights, int numPixels, int numChannels) {
        for (int i = 0; i < numPixels; i++) {
            if (weights[i] > 0) {
                const float inv = 1.0f / weights[i];
                float *p = &pixels[i * numChannels];
                for (int j = 0; j < numChannels; j++) {
                    p[j] *= inv;
                }
            }
        }
    }
};

#endif
