/**
 * Project: openRender
 *
 * File: compositor.h
 *
 * Description:
 *   Shared transparency/matte compositor (spec 008-hider-parity-convergence,
 *   R3). One implementation of front-to-back "over" compositing, matte
 *   carve-out, and comp/non-comp AOV rules, ported verbatim from
 *   CStochastic::rasterEnd (stochastic.cpp) so CStochastic and CRaytracer
 *   stop diverging (D6, D8). See contracts/compositor-contract.md.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "common/algebra.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CompositeSample
// Description			:	One sample to be composited -- a reyes CFragment
//							or a raytrace ray-hit, converted by the caller.
//							The compositor never reads either native structure
//							directly (FR-010: CFragment's layout is frozen).
//							Matte is the existing negative-opacity convention:
//							opacity[i] < 0 for any channel i.
// Comments				:	color/opacity point at 3 floats owned by the
//							caller (CFragment::color/opacity, or a raytrace
//							CRay's equivalent); extraSamples points at that
//							same sample's extra-channel storage, indexed by
//							CRenderer::compChannelOrder / nonCompChannelOrder
//							offsets. No ownership transfer, no allocation.
class CompositeSample {
public:
    const float *color;
    const float *opacity;
    const float *extraSamples;
    float z;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CompositeAccumulator
// Description			:	Running "over" compositing state, owned by the
//							caller so its lifetime can match either hider:
//							reyes keeps one as a stack local per raster-sample
//							position (reset every position via begin());
//							raytrace stores one per in-flight CPrimaryRay so
//							it survives across postShade() calls at increasing
//							continuation depth.
// Comments				:	`extra` must be sized/offset-compatible with
//							CRenderer::compChannelOrder (i.e. large enough to
//							hold CRenderer::numExtraSamples floats at the
//							offsets that table describes) -- it is the same
//							buffer the caller will also pass non-comp AOVs
//							into via CCompositor::compositeNonComp.
class CompositeAccumulator {
public:
    vector color;
    vector opacity;
    vector ropacity;
    float *extra;
    bool hasBase;
};

enum class DepthFilterMode { Min, Max, Avg, Mid };

///////////////////////////////////////////////////////////////////////
// Class				:	CCompositor
// Description			:	Single shared implementation of transparency/
//							matte compositing (SC-004: verifiable by
//							inspecting this file alone). Does not change
//							matte/transparency semantics, only unifies which
//							code path evaluates them (FR-029 -- no new RIB
//							token/attribute/option).
// Comments				:	composite()/compositeNonComp() are R3 (Phase 5)
//							scope. evaluateDepth() is declared here to match
//							contracts/compositor-contract.md's interface, but
//							implemented in Phase 6/S3 (T037-T046) -- do not
//							call it before then.
class CCompositor {
public:
    // Resets `acc` to the "no samples yet" state. `extra` is caller-owned
    // (see CompositeAccumulator::extra); its contents are undefined until
    // the first composite() call supplies a base sample.
    static void begin(CompositeAccumulator &acc, float *extra);

    // Composites one sample into `acc`. Must be called in strict
    // front-to-back (nearest-to-farthest) order, one call per fragment/hit.
    // Ports CStochastic::rasterEnd's compositeSampleLoop over/matte
    // carve-out math verbatim (stochastic.cpp:660-796): the first call after
    // begin() is the base sample (straight assign, or zero+carve-out if
    // matte); every subsequent call accumulates via the running `ropacity`
    // remainder (over-composite, or matte carve-out if that sample is
    // matte). This is FR-002/FR-011's single shared implementation.
    static void composite(CompositeAccumulator &acc, const CompositeSample &sample);

    // Non-composited AOVs: tests one candidate sample against the applicable
    // z-visibility threshold, latching its AOVs into `dst` on success. Ports
    // stochastic.cpp:534-596 verbatim, including its existing FIXME'd
    // asymmetry (checkMatteZThreshold is applied whenever the pixel contains
    // any matte fragment, not only when the tested sample itself is matte --
    // preserved as-is, not "fixed", since correctness here means bit-for-bit
    // parity with reyes today). `dst` is the same extra-channel buffer passed
    // as `acc.extra` elsewhere; `pixelHasMatte` mirrors CFragment list's
    // sentinel check (cPixel->first.opacity[i] < 0 for any i) and is fixed
    // for the whole pixel (matches the original code's outer if/else, not a
    // per-sample decision). Returns true when the sample passed its
    // threshold and its AOVs were latched -- the caller (walking the
    // fragment list front-to-back) should then record this sample's z and
    // stop; false means keep walking to the next fragment.
    static bool compositeNonComp(float *dst, const CompositeSample &sample,
                                  const vector zvisibilityThreshold, bool pixelHasMatte);

    // Depth-filter modes (min/max/avg/mid) + zvisibilityThreshold exclusion,
    // shared by both hiders -- Phase 6/S3 (T037-T042). Resolves ONE native
    // sample point's front-to-back candidate list (one reyes subsample's
    // fragment list, or one raytrace ray's continuation-hit chain) into a
    // single z, ported verbatim from CStochastic::rasterEnd's per-subsample
    // Z[0]/Z2[0] resolution (stochastic.cpp:521-624). `zs[i]`/`opacities[i*3]`
    // are candidate i's z and RGB opacity, front-to-back (nearest first),
    // `count` candidates total.
    //
    // `pixelHasMatte` reproduces an asymmetry present in the original reyes
    // code (not a new one): the FIRST candidate's threshold test uses the
    // pixel-wide matte flag (matching compositeNonComp -- one fragment
    // anywhere in the pixel being matte switches the formula for all
    // candidates), but Mid mode's SECOND-candidate search uses a per-candidate
    // matte test instead (each candidate's own opacity sign). Both are
    // preserved exactly as-is, not unified, since parity with today's reyes
    // output -- including this quirk -- is the correctness bar (T041).
    //
    // This is deliberately NOT the cross-subsample/cross-sample min/max/avg
    // grid reduction -- that stays in each hider's own loop (reyes's
    // existing switch(depthFilter) in stochastic.cpp:680-818 is untouched
    // generic numeric aggregation over this function's per-subsample
    // return value; raytrace's call site does the analogous reduction over
    // its own lens/time samples). Stage-1-vs-stage-2 split confirmed via
    // research.md's S3 decision ("a shared depth-filter function taking a
    // list of candidate (z, opacity) pairs and a mode enum" -- a pure port,
    // no new algorithm).
    //
    // `zold` floors the Mid-mode second candidate exactly as CPixel::zold
    // does in reyes (an early-rejection z tracked during rasterization,
    // stochastic.h:86); pass -C_INFINITY (the default) from call sites with
    // no equivalent notion (e.g. raytrace) to make the clamp a no-op.
    static float evaluateDepth(const float *zs, const float *opacities, int count,
                                DepthFilterMode mode, const vector zvisibilityThreshold,
                                bool pixelHasMatte, float zold = -1e30f);
};

#endif
