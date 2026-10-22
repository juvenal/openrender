/**
 * Project: openRender
 *
 * File: samplingPmj02.cpp
 *
 * Description:
 *   The PMJ02 backend of the sampler library: 64 progressive multi-jittered
 *   (0,2) patterns of 4096 points, each built once, deterministically, on
 *   first use (see sampling.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <algorithm>
#include <bit>
#include <mutex>
#include <vector>

#include "sampling.h"

namespace openrender::sampling::detail {

    namespace {

        constexpr uint32_t kPatterns = kPmj02Patterns;
        constexpr uint32_t kPmjSalt1D = 0x91D1D1D1u;
        constexpr uint32_t kPmjSalt2D = 0x92D2D2D2u;
        constexpr uint64_t kPmjSeed = 0x504D4A3032ull; // "PMJ02", the fixed internal seed
        constexpr int kLevels = 12;                    // 2^12 = 4096 points
        constexpr int kMaxAttempts = 64;               // restarts of one doubling step
        constexpr long kNodeLimit = 1 << 22;           // search nodes for one new point

        // PMJ02 generation, after Christensen, Kensler and Kilpatrick 2018
        // ("Progressive Multi-Jittered Sample Sequences") with the admissible-region
        // search of Pharr 2019 ("Efficient Generation of Points that Satisfy
        // Two-Dimensional Elementary Intervals").
        //
        // A pattern is built by doubling: from the n = 2^k points placed so far, the
        // next n are generated, one for each existing point i, so that the first 2n
        // points are stratified with respect to every base-2 elementary interval
        // with area 1/(2n) (a (0, k+1, 2)-net). Coordinates are 32-bit fixed-point
        // fractions, so "the first d digits" is a right shift.
        //
        //  - Interval choice (Christensen et al.): for an even k (n = 4^j, a grid of
        //    2^j x 2^j square cells with one point each) the new point goes in the
        //    same cell as point i and in the sub-quadrant diagonally opposite to it;
        //    for an odd k it goes in the same 2^j x 2^j cell as point i, in whichever
        //    sub-quadrant stays admissible.
        //  - Admissible positions (Pharr): the occupancy of every elementary interval
        //    is one boolean (n(1 + log2 n) of them). A candidate position is valid
        //    when none of the k + 2 elementary intervals that contain it is occupied.
        //    Here the position is found by a depth-first search that refines the x
        //    and y digits alternately and checks every interval whose shape is fully
        //    determined by the digits chosen so far, so that dead ends are cut early.
        //    The remaining digits (below the 2n-level strata) are random.
        //
        // The randomness is a counter-based hash of the pattern number, so the table
        // is the same on every run and every thread.
        class Generator {
            public:
                explicit Generator(uint64_t seed) : state(seed) {}

                // Fills pts[0 .. 2^kLevels) ; false if a doubling step could not be completed.
                bool generate(uint32_t (*pts)[2]) {
                    pts[0][0] = rnd();
                    pts[0][1] = rnd();
                    for (k = 0; k < kLevels; ++k) {
                        bool done = false;
                        for (int attempt = 0; attempt < kMaxAttempts && !done; ++attempt)
                            done = doubleOnce(pts);
                        if (!done)
                            return false;
                    }
                    return true;
                }

            private:
                uint64_t state;
                int k = 0;                // the step doubles 2^k points to 2^(k+1)
                std::vector<uint8_t> occ; // occ[a] = occupancy of the (a, k+1-a) intervals
                uint32_t foundX = 0, foundY = 0;
                long nodes = 0;

                uint32_t rnd() {
                    state += 0x9E3779B97F4A7C15ull;
                    return static_cast<uint32_t>(finalize64(state) >> 32);
                }

                static uint32_t top(uint32_t v, int digits) { return digits ? v >> (32 - digits) : 0u; }

                // Index of the occupancy bit of the (a, b = k+1-a) interval holding (x, y).
                size_t slot(int a, uint32_t xDigits, uint32_t yDigits) const {
                    return (static_cast<size_t>(a) << (k + 1)) + ((static_cast<size_t>(xDigits) << (k + 1 - a)) | yDigits);
                }

                void mark(uint32_t x, uint32_t y) {
                    for (int a = 0; a <= k + 1; ++a)
                        occ[slot(a, top(x, a), top(y, k + 1 - a))] = 1;
                }

                // x holds the first lx digits and y the first ly digits of the candidate.
                // Every interval shape (a, k+1-a) with a <= lx and k+1-a <= ly is known.
                bool admissible(int lx, int ly, uint32_t x, uint32_t y) const {
                    const int lo = std::max(0, k + 1 - ly), hi = std::min(lx, k + 1);
                    for (int a = lo; a <= hi; ++a) {
                        const int b = k + 1 - a;
                        const uint32_t xa = a ? x >> (lx - a) : 0u;
                        const uint32_t yb = b ? y >> (ly - b) : 0u;
                        if (occ[slot(a, xa, yb)])
                            return false;
                    }
                    return true;
                }

                bool search(int lx, int ly, uint32_t x, uint32_t y) {
                    if (++nodes > kNodeLimit || !admissible(lx, ly, x, y))
                        return false;
                    if (lx == k + 1 && ly == k + 1) {
                        foundX = x;
                        foundY = y;
                        return true;
                    }
                    const bool extendX = (lx <= ly && lx < k + 1) || ly == k + 1;
                    const uint32_t first = rnd() & 1u;
                    for (uint32_t t = 0; t < 2; ++t) {
                        const uint32_t bit = t ^ first;
                        if (extendX ? search(lx + 1, ly, (x << 1) | bit, y) : search(lx, ly + 1, x, (y << 1) | bit))
                            return true;
                    }
                    return false;
                }

                bool doubleOnce(uint32_t (*pts)[2]) {
                    const uint32_t n = 1u << k;
                    occ.assign(static_cast<size_t>(k + 2) << (k + 1), 0);
                    for (uint32_t i = 0; i < n; ++i)
                        mark(pts[i][0], pts[i][1]);

                    const int j = k / 2;
                    const bool even = (k % 2 == 0);
                    const int fixedDigits = even ? j + 1 : j;
                    for (uint32_t i = 0; i < n; ++i) {
                        uint32_t x0 = top(pts[i][0], fixedDigits), y0 = top(pts[i][1], fixedDigits);
                        if (even) { // the diagonally opposite sub-quadrant of the cell
                            x0 ^= 1u;
                            y0 ^= 1u;
                        }
                        nodes = 0;
                        if (!search(fixedDigits, fixedDigits, x0, y0))
                            return false;
                        // The digits below the (k+1)-level strata are free.
                        const uint32_t qx = (foundX << (31 - k)) | (rnd() >> (k + 1));
                        const uint32_t qy = (foundY << (31 - k)) | (rnd() >> (k + 1));
                        pts[n + i][0] = qx;
                        pts[n + i][1] = qy;
                        mark(qx, qy);
                    }
                    return true;
                }
        };

        // A pattern is built on first use, once, by whichever thread gets there first;
        // the others wait. 64 patterns of 4096 points are about 2 MB of fixed-point
        // coordinates (untouched pages cost nothing).
        struct Table {
                std::once_flag once[kPatterns];
                uint32_t points[kPatterns][kPmj02Points][2];
        };

        void build(Table *t, uint32_t p) {
            Generator g(mix(kPmjSeed, p));
            if (g.generate(t->points[p]))
                return;
            // Not reached in practice (Sampling_PMJ02Construction would fail on a fallback pattern). Should a
            // step ever fail, fall back to an Owen-scrambled Sobol (0,2)-sequence for
            // this pattern: deterministic, and it keeps every net property.
            const uint64_t h = mix(kPmjSeed, p, 0x50B01ull);
            for (uint32_t i = 0; i < kPmj02Points; ++i) {
                t->points[p][i][0] = nestedUniformScramble(sobolDim0(i), static_cast<uint32_t>(h));
                t->points[p][i][1] = nestedUniformScramble(sobolDim1(i), static_cast<uint32_t>(h >> 32));
            }
        }

        const uint32_t (*pattern(uint32_t p))[2] {
            static Table table;
            std::call_once(table.once[p], build, &table, p);
            return table.points[p];
        }

        // The point index is shuffled so that dimensions do not all walk a pattern in
        // the same order. A PMJ02 sequence is only guaranteed for its *prefixes* (the
        // first 2^m points are a (0, m, 2)-net; an arbitrary aligned block is not), so
        // the shuffle must map every prefix onto itself: the indices [2^t, 2^(t+1)) are
        // permuted among themselves, by a nested uniform permutation of their t low
        // bits, and index 0 stays. The first 2^m draws are then the first 2^m points
        // of the pattern, in a different order.
        uint32_t shuffleIndex(uint32_t index, uint32_t seed) {
            if (index < 2)
                return index;
            const int t = 31 - std::countl_zero(index);
            const uint32_t low = index - (1u << t);
            const uint32_t shuffled = nestedUniformScramble(low << (32 - t), seed + static_cast<uint32_t>(t) * 0x9E3779B9u) >> (32 - t);
            return (1u << t) + shuffled;
        }

    } // namespace

    Sample2D pmj02Point(uint32_t patternIndex, uint32_t j) {
        const uint32_t *pt = pattern(patternIndex % kPatterns)[j % kPmj02Points];
        return Sample2D{toUnitFloat(pt[0]), toUnitFloat(pt[1])};
    }

    float pmj02Get1D(uint64_t seedHash, uint32_t dim, uint32_t index) {
        if (index >= kPmj02Points)
            return sobolGet1D(seedHash, dim, index);
        const uint64_t h = mix(seedHash, dim, kPmjSalt1D);
        const uint64_t g = finalize64(h);
        const uint32_t j = shuffleIndex(index, static_cast<uint32_t>(h >> 32));
        const uint32_t p = static_cast<uint32_t>(h) % kPatterns;
        return toUnitFloat(nestedUniformScramble(pattern(p)[j][0], static_cast<uint32_t>(g)));
    }

    Sample2D pmj02Get2D(uint64_t seedHash, uint32_t dim, uint32_t index) {
        if (index >= kPmj02Points)
            return sobolGet2D(seedHash, dim, index);
        const uint64_t h = mix(seedHash, dim, kPmjSalt2D);
        const uint64_t g = finalize64(h);
        const uint32_t j = shuffleIndex(index, static_cast<uint32_t>(h >> 32));
        const uint32_t p = static_cast<uint32_t>(h) % kPatterns;
        const uint32_t *pt = pattern(p)[j];
        return Sample2D{toUnitFloat(nestedUniformScramble(pt[0], static_cast<uint32_t>(g))),
                        toUnitFloat(nestedUniformScramble(pt[1], static_cast<uint32_t>(g >> 32)))};
    }

} // namespace openrender::sampling::detail
