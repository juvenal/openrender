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

#include <random>

#include "common/algebra.h" // absf()
#include "random.h"         // sampleDisk()

namespace {
// Only needs to differ across buckets within a frame, not be cryptographic.
unsigned int bucketSeed(int left, int top) {
    unsigned int h = 2166136261u; // FNV-1a
    h = (h ^ (unsigned int)left) * 16777619u;
    h = (h ^ (unsigned int)top) * 16777619u;
    return h;
}
} // namespace

CSampleValue CSampler::nextSample(int timeLinearIndex, int importanceLinearIndex, bool wantLens) {
    CSampleValue sample;

    sample.jitterX = jitteredOffset();
    sample.jitterY = jitteredOffset();

    const float totalSamples = (float)(samplesX * samplesY);
    sample.timeStratum = (timeLinearIndex + jitteredOffset()) / totalSamples;
    sample.importance = 1.0f - (importanceLinearIndex + jitteredOffset()) / totalSamples;

    if (wantLens) {
        float lens[2];
        sampleDisk(lens, lensFn);
        sample.lensU = lens[0];
        sample.lensV = lens[1];
    } else {
        sample.lensU = 0.0f;
        sample.lensV = 0.0f;
    }

    return sample;
}

std::vector<CSampleValue> CSampler::generateBucketTable(int left, int top, int sampleWidth, int sampleHeight,
                                                          int xSampleOffset, int ySampleOffset, bool wantLens) {
    std::mt19937 rng(bucketSeed(left, top));
    auto detUrand = [&rng]() -> float {
        // mt19937's range is exactly [0, 2^32) per the C++ standard.
        return (float)(rng() / 4294967296.0);
    };
    auto detLens = [&detUrand](float *s) {
        s[0] = detUrand();
        s[1] = detUrand();
    };
    CSampler det(jitterAmount, samplesX, samplesY, detUrand, detLens);

    std::vector<CSampleValue> table((size_t)sampleWidth * (size_t)sampleHeight);

    // Faithfully ports stochastic.cpp rasterBegin's phase-shifted pxi/pxj
    // wraparound so the table's bucket-local (row, col) entries are exactly
    // the values reyes would have drawn for that position -- raytrace then
    // reads the same entries by its own (row, col) tile position (T082),
    // with no need to replicate this phase shift on its side.
    int pxi = samplesY - ySampleOffset;
    for (int i = 0; i < sampleHeight; i++, pxi++) {
        if (pxi >= samplesY) {
            pxi = 0;
        }
        int pxj = samplesX - xSampleOffset;
        for (int j = 0; j < sampleWidth; j++, pxj++) {
            if (pxj >= samplesX) {
                pxj = 0;
            }
            table[(size_t)i * sampleWidth + j] = det.nextSample(
                pxi * samplesX + pxj,
                pxj * samplesY + pxi,
                wantLens);
        }
    }
    return table;
}

float CSampler::circleOfConfusion(float z, float invFocaldistance, float cocFactorSamples) {
    return absf((1.0f / z) - invFocaldistance) * cocFactorSamples;
}
