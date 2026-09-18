/**
 * Project: openRender
 *
 * File: sampler.h
 *
 * Description:
 *   Shared per-pixel-sample generator (jitter xy, motion-blur time
 *   stratum, lens/aperture point, importance weight), consumed by both
 *   CStochastic::rasterBegin and CRaytracer::computeSamples so the two
 *   hiders' sampling math has a single home instead of two independently
 *   drifting implementations (spec 008-hider-parity-convergence, R2/S1).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SAMPLER_H
#define SAMPLER_H

#include <functional>
#include <utility>
#include <vector>

#include "random.h" // sampleDisk()

///////////////////////////////////////////////////////////////////////
// CSampleValue - one pixel-sample's worth of jitter/time/lens/importance.
///////////////////////////////////////////////////////////////////////
struct CSampleValue {
    float jitterX, jitterY; // Sub-pixel offset, single canonical constant
    float timeStratum;      // Motion-blur time sample, stratified in [0,1)
    float lensU, lensV;     // Lens/aperture point, from sampleDisk()
    float importance;       // Sample weight
};

///////////////////////////////////////////////////////////////////////
// CSampler
//
// Deviation from contracts/sampler-contract.md's zero-arg `nextSample()`:
// CStochastic and CRaytracer visit their sample grids in different loop
// orders (stochastic: nested pxi/pxj with xSampleOffset wraparound;
// raytracer: tiled 8x8 sub_x/sub_y), and jt/jimp use different linear-index
// orderings even within stochastic itself. The stratum position is
// therefore a loop-carried value owned by each hider's own loop, not
// CSampler state -- nextSample() takes it as an argument instead of
// deriving it from an internal counter, which would desync the two hiders'
// existing (intentionally different) traversal order.
//
// Similarly takes two separate generator callables (matching what each
// hider already has) rather than the contract's single "Rng &rng": jitter
// draws come from a scalar urand()-like source, while the lens point comes
// from a distinct 2D generator (reyes: a per-pixel CSobol<2> sequence;
// raytrace: a pair of urand() calls) -- collapsing these into one type
// would force a needless RNG-type unification the contract explicitly
// calls a non-goal.
///////////////////////////////////////////////////////////////////////
class CSampler {
public:
    // The canonical near-0.5 stratified-jitter offset. Deliberately not
    // exactly 0.5f: avoids samples landing exactly on a stratum/pixel
    // boundary. Previously duplicated (and drifted) as raytracer.cpp's
    // spatial `+0.5f` vs. stochastic.cpp's `0.5001011f` (jx/jt/jimp) and a
    // divergent `0.5001017f` typo in stochastic.cpp's jy (D2).
    static constexpr float kJitterConstant = 0.5001011f;

    // jitterAmount: CRenderer::jitter. samplesX/samplesY: CRenderer::
    // pixelXsamples/pixelYsamples (the stratification grid both jt/jimp
    // normalize against). urandFn: () -> float in [0,1). lensFn: the
    // sampleDisk()-compatible callable writing a float[2] pair of
    // independent U(0,1) values.
    template <typename UrandFn, typename LensFn>
    CSampler(float jitterAmount, int samplesX, int samplesY, UrandFn &&urandFn, LensFn &&lensFn)
        : jitterAmount(jitterAmount), samplesX(samplesX), samplesY(samplesY),
          urandFn(std::forward<UrandFn>(urandFn)), lensFn(std::forward<LensFn>(lensFn)) {}

    // timeLinearIndex / importanceLinearIndex: the caller's own linear
    // stratum index (stochastic's jt uses pxi*samplesX+pxj; its jimp uses
    // pxj*samplesY+pxi -- preserve each ordering verbatim at the call
    // site, do not unify them). wantLens: skip the disk sample entirely
    // when CRenderer::aperture == 0 / OPTIONS_FLAGS_FOCALBLUR is unset.
    CSampleValue nextSample(int timeLinearIndex, int importanceLinearIndex, bool wantLens);

    // Lens/aperture point only, via sampleDisk() (S1). For callers whose
    // architecture generates the lens sample at a different pipeline stage
    // than jitter/time -- raytrace's DOF disk sample happens in
    // computeSamples(), once primary rays are already batched, well after
    // sample()'s per-ray jitter/time generation -- so paying for a full
    // nextSample() call (3 unused jitteredOffset() draws) just to reach the
    // lens fields would be wasteful.
    void lensSample(float *result) { sampleDisk(result, lensFn); }

    // Option B (gated via CRenderer::correlatedSampleTable, internal-only --
    // FR-027): both hiders regenerate -- not share memory for, since the
    // parity harness runs each hider as a separate OS process -- the
    // identical table for a given bucket by seeding a deterministic RNG
    // from the bucket's own raster-space origin (left, top) instead of
    // drawing from this instance's own live, per-hider urandFn/lensFn
    // (reyes: CSobol<2>; raytrace: urand()). std::mt19937's output sequence
    // is exactly specified by the C++ standard, so the same seed reproduces
    // bit-identical floats in both processes; jitterAmount/samplesX/
    // samplesY are already identical between hiders (both read the same
    // CRenderer statics), so only the *source* of randomness needs pinning
    // here, not the formula.
    //
    // Deviation from contracts/sampler-contract.md's `(bucketId,
    // sampleCount)` signature: a flat sample count can't reproduce
    // stochastic's phase-shifted pxi/pxj wraparound (stochastic.cpp's
    // rasterBegin), so the table spans the bucket's full (sampleWidth x
    // sampleHeight) grid, indexed by bucket-local (row, col) -- the same
    // domain both hiders already iterate for a shared bucket dispatch
    // (stochastic: outer i / inner j; raytrace: tiled i+x / j+y) -- rather
    // than a linear sample index.
    std::vector<CSampleValue> generateBucketTable(int left, int top, int sampleWidth, int sampleHeight,
                                                   int xSampleOffset, int ySampleOffset, bool wantLens);

    // S1: canonical circle-of-confusion formula (reyes's cocSamples(z)
    // macro, renderer.h:39), reyes's sole consumer -- raytrace's DOF model
    // offsets the ray origin directly by CRenderer::aperture (a true lens
    // ray, not a screen-space CoC scatter: D9, a documented non-closable
    // residual), so it has no use for this formula.
    static float circleOfConfusion(float z, float invFocaldistance, float cocFactorSamples);

private:
    float jitterAmount;
    int samplesX, samplesY;
    std::function<float()> urandFn;
    std::function<void(float *)> lensFn;

    float jitteredOffset() { return jitterAmount * (urandFn() - 0.5f) + kJitterConstant; }
};

#endif
