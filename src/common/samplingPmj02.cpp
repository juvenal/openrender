/**
 * Project: openRender
 *
 * File: samplingPmj02.cpp
 *
 * Description:
 *   The PMJ02 backend of the sampler library: 64 patterns of 4096 points,
 *   built once, deterministically, on first use (see sampling.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <mutex>

#include "sampling.h"

namespace openrender::sampling::detail {

    namespace {

        constexpr uint32_t kPatterns = 64;
        constexpr uint32_t kPmjSalt1D = 0x91D1D1D1u;
        constexpr uint32_t kPmjSalt2D = 0x92D2D2D2u;
        constexpr uint64_t kPmjSeed = 0x504D4A3032ull; // "PMJ02", the fixed internal seed

        // Each pattern is a progressive (0,2)-sequence of 4096 points: every
        // power-of-two prefix is a (0, m, 2)-net. Points are kept as 32-bit
        // fixed-point fractions so that the runtime Owen scramble is exact.
        //
        // The patterns are Owen-scrambled Sobol (0,2)-sequences, one scramble per
        // pattern, not the output of Christensen et al.'s PMJ02 generator. They give
        // the property the tests and the spec state (G4), and share the Sobol
        // backend's stratification at power-of-two prefixes.
        struct Table {
                uint32_t points[kPatterns][kPmj02Points][2];
        };

        void build(Table *t) {
            for (uint32_t p = 0; p < kPatterns; ++p) {
                const uint64_t h = mix(kPmjSeed, p);
                const uint32_t sx = static_cast<uint32_t>(h), sy = static_cast<uint32_t>(h >> 32);
                for (uint32_t j = 0; j < kPmj02Points; ++j) {
                    t->points[p][j][0] = nestedUniformScramble(sobolDim0(j), sx);
                    t->points[p][j][1] = nestedUniformScramble(sobolDim1(j), sy);
                }
            }
        }

        const Table &table() {
            static Table t;
            static std::once_flag once;
            std::call_once(once, build, &t);
            return t;
        }

        // The point index is shuffled by a nested uniform permutation of its 12 bits:
        // it maps every aligned block of 2^m indices onto an aligned block of the same
        // size, so the first 2^m draws stay a (0, m, 2)-net.
        uint32_t shuffleIndex(uint32_t index, uint32_t seed) {
            return nestedUniformScramble(index << 20, seed) >> 20;
        }

    } // namespace

    Sample2D pmj02Point(uint32_t pattern, uint32_t j) {
        const uint32_t *pt = table().points[pattern % kPatterns][j % kPmj02Points];
        return Sample2D{toUnitFloat(pt[0]), toUnitFloat(pt[1])};
    }

    float pmj02Get1D(uint64_t seedHash, uint32_t dim, uint32_t index) {
        if (index >= kPmj02Points)
            return sobolGet1D(seedHash, dim, index);
        const uint64_t h = mix(seedHash, dim, kPmjSalt1D);
        const uint64_t g = finalize64(h);
        const uint32_t j = shuffleIndex(index, static_cast<uint32_t>(h >> 32));
        const uint32_t pattern = static_cast<uint32_t>(h) % kPatterns;
        return toUnitFloat(nestedUniformScramble(table().points[pattern][j][0], static_cast<uint32_t>(g)));
    }

    Sample2D pmj02Get2D(uint64_t seedHash, uint32_t dim, uint32_t index) {
        if (index >= kPmj02Points)
            return sobolGet2D(seedHash, dim, index);
        const uint64_t h = mix(seedHash, dim, kPmjSalt2D);
        const uint64_t g = finalize64(h);
        const uint32_t j = shuffleIndex(index, static_cast<uint32_t>(h >> 32));
        const uint32_t pattern = static_cast<uint32_t>(h) % kPatterns;
        const uint32_t *pt = table().points[pattern][j];
        return Sample2D{toUnitFloat(nestedUniformScramble(pt[0], static_cast<uint32_t>(g))),
                        toUnitFloat(nestedUniformScramble(pt[1], static_cast<uint32_t>(g >> 32)))};
    }

} // namespace openrender::sampling::detail
