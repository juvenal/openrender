/**
 * Project: openRender
 *
 * File: sampler.cpp
 *
 * Description:
 *   CSampler implementation. See sampler.h for the design rationale.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "sampler.h"

#include <cmath>

#include "common/algebra.h" // absf()

float CSampler::circleOfConfusion(float z, float invFocaldistance, float cocFactorSamples) {
    return absf((1.0f / z) - invFocaldistance) * cocFactorSamples;
}

///////////////////////////////////////////////////////////////////////
// Spec 039, Phase 5 (R3.1): camera samples from the sampler's slots
///////////////////////////////////////////////////////////////////////

int CSampler::reyesAbsSub(int origin, int index) {
    return origin + index;
}

int CSampler::raytraceAbsSub(int originPixels, int samplesPerPixel, int sampleOffset, int index) {
    return originPixels * samplesPerPixel - sampleOffset + index;
}

namespace {
    // Shirley-Chiu concentric map of the unit square onto the unit disk. It is area
    // preserving and continuous, needs no rejection (so one slot sample gives one lens
    // point) and keeps the stratification of the lens slot on the lens.
    void concentricDisk(float u, float v, float *result) {
        const float a = 2.0f * u - 1.0f, b = 2.0f * v - 1.0f;
        if (a == 0.0f && b == 0.0f) {
            result[0] = 0.0f;
            result[1] = 0.0f;
            return;
        }
        constexpr float kPi = 3.14159265358979323846f;
        float r, phi;
        if (absf(a) > absf(b)) {
            r = a;
            phi = (kPi / 4.0f) * (b / a);
        }
        else {
            r = b;
            phi = kPi / 2.0f - (kPi / 4.0f) * (a / b);
        }
        result[0] = r * std::cos(phi);
        result[1] = r * std::sin(phi);
    }
} // namespace

CSampleValue CSampler::cameraSample(const openrender::sampling::CSamplingConfig &config, int absSubX, int absSubY, bool wantLens) {
    return cameraSample(config, openrender::sampling::cameraSeeds(config), absSubX, absSubY, wantLens);
}

CSampleValue CSampler::cameraSample(const openrender::sampling::CSamplingConfig &config, const openrender::sampling::CameraSeeds &seeds, int absSubX,
                                    int absSubY, bool wantLens) {
    using namespace openrender::sampling;

    // The subpixel's identity: the pixel it is in (floor division, because the
    // margin of the border buckets has negative coordinates) and its position in the
    // pixel. The position is also the time stratum (row * nx + column); the
    // importance uses the transposed one (column * ny + row), as reyes always has.
    SampleId id;
    id.px = floorDiv(absSubX, config.nx);
    id.py = floorDiv(absSubY, config.ny);
    const int column = floorMod(absSubX, config.nx), row = floorMod(absSubY, config.ny);
    id.sampleIndex = static_cast<uint32_t>(row * config.nx + column);
    id.frame = config.frame;
    id.seed = config.seed;

    // jitter * (u - 0.5) + kJitterConstant, with u from the camera slots.
    auto offset = [&](float u) { return config.jitter * (u - 0.5f) + kJitterConstant; };
    const float totalSamples = static_cast<float>(config.nx * config.ny);

    CSampleValue sample;
    const CameraDraws draws = cameraDraws(config, seeds, id, wantLens);
    sample.jitterX = offset(draws.film.u);
    sample.jitterY = offset(draws.film.v);
    sample.timeStratum = (static_cast<float>(row * config.nx + column) + offset(draws.time)) / totalSamples;
    sample.importance = 1.0f - (static_cast<float>(column * config.ny + row) + offset(draws.importance)) / totalSamples;

    if (wantLens) {
        float disk[2];
        concentricDisk(draws.lens.u, draws.lens.v, disk);
        sample.lensU = disk[0];
        sample.lensV = disk[1];
    }
    else {
        sample.lensU = 0.0f;
        sample.lensV = 0.0f;
    }
    sample.key = cameraKey(id);
    return sample;
}
