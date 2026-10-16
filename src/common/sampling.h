/**
 * Project: openRender
 *
 * File: sampling.h
 *
 * Description:
 *   The sampler library (spec 039): pure 1D/2D draws for three backends,
 *   the identity keys they are seeded from, the camera slots, and the
 *   local keyed stream. Everything here is a function of its arguments:
 *   there is no mutable global state (the PMJ02 tables are built once,
 *   thread-safely, and never change afterwards).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	sampling.h
//  Classes				:	CSamplingConfig, CKeyedStream
//  Description			:	Counter-based, Owen-scrambled Sobol / PMJ02 /
//                          independent sample generation
//
////////////////////////////////////////////////////////////////////////
#ifndef SAMPLING_H
#define SAMPLING_H

#include <bit>
#include <cstdint>

namespace openrender::sampling {

    enum class Backend : uint8_t {
        Sobol,
        PMJ02,
        Independent,
        Legacy // internal: the pre-039 generators; deleted in Phase 9
    };

    // One camera sample. px/py are the absolute raster pixel of the subpixel
    // sample (negative in the margin), sampleIndex is its position inside the
    // pixel's nx * ny samples.
    struct SampleId {
            int32_t px, py;
            uint32_t sampleIndex, frame, seed;
    };

    enum class SampleKind : uint8_t { Camera,
                                      Cell,
                                      Child };

    // The 64-bit hashed form of an identity. `path` never mixes in the camera
    // sampleIndex; `pixelSampleIndex` is -1 for keys that descend from a cell.
    struct SampleKey {
            uint64_t full, path;
            int32_t pixelSampleIndex;
            uint8_t kind; // SampleKind; debug and test only, never hashed
    };

    struct SubstreamKey {
            uint64_t seedHash;
            uint32_t indexBase;
    };

    struct Sample2D {
            float u, v;
    };
    struct Sample3D {
            float u, v, w;
    };
    struct Sample4D {
            float u, v, w, x;
    };

    // Immutable per frame; built by the renderer core.
    struct CSamplingConfig {
            Backend backend;
            uint32_t seed, frame;
            int nx, ny;
            float jitter;
    };

    // Camera slots: the fixed dimension layout shared by every hider (FR-009).
    enum CameraSlot : uint32_t { Film2D = 0,
                                 Lens2D = 1,
                                 Time1D = 2,
                                 Importance1D = 3 };

    // Implementation details: hashing, the Owen/Sobol machinery, the PMJ02 tables and
    // the test-only entry points. Nothing outside the sampler library and its tests
    // names anything in here.
    namespace detail {

        ///////////////////////////////////////////////////////////////////////
        // Hashing
        ///////////////////////////////////////////////////////////////////////

        // SplitMix64 finalizer.
        inline uint64_t finalize64(uint64_t x) {
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        }

        // Combines a hash with one more value. Asymmetric, so mix(mix(h, a), b) is
        // order dependent.
        inline uint64_t mix(uint64_t h, uint64_t v) {
            return finalize64(h ^ finalize64(v + 0x9E3779B97F4A7C15ull));
        }
        inline uint64_t mix(uint64_t h, uint64_t a, uint64_t b) { return mix(mix(h, a), b); }
        inline uint64_t mix(uint64_t h, uint64_t a, uint64_t b, uint64_t c) { return mix(mix(h, a, b), c); }
        inline uint64_t mix(uint64_t h, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
            return mix(mix(h, a, b, c), d);
        }

        // The top 24 bits of a 32-bit value as a float in [0, 1 - 2^-24] (never 1.0).
        inline float toUnitFloat(uint32_t bits) {
            return static_cast<float>(bits >> 8) * 0x1p-24f;
        }

        ///////////////////////////////////////////////////////////////////////
        // Owen-scrambled (0,2) machinery (Burley 2020)
        ///////////////////////////////////////////////////////////////////////

        inline uint32_t reverseBits32(uint32_t x) {
            x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
            x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
            x = ((x >> 4) & 0x0F0F0F0Fu) | ((x & 0x0F0F0F0Fu) << 4);
            x = ((x >> 8) & 0x00FF00FFu) | ((x & 0x00FF00FFu) << 8);
            return (x >> 16) | (x << 16);
        }

        // Flips each bit as a function of the bits below it.
        inline uint32_t laineKarras(uint32_t x, uint32_t seed) {
            x += seed;
            x ^= x * 0x6c50b47cu;
            x ^= x * 0xb82f1e52u;
            x ^= x * 0xc7afe638u;
            x ^= x * 0x8d22f6e6u;
            return x;
        }

        // Nested uniform (Owen) scramble: bit k is flipped by a function of the bits
        // above it. Applied to a fixed-point fraction it is an Owen scramble. Applied
        // to a sample index it permutes every aligned block of 2^m indices within
        // itself and moves the first block onto another aligned block, so the first
        // 2^m indices keep their (0, m, 2) structure (this is what decorrelates
        // dimensions without losing stratification).
        inline uint32_t nestedUniformScramble(uint32_t x, uint32_t seed) {
            return reverseBits32(laineKarras(reverseBits32(x), seed));
        }

        // Sobol dimension 0 (van der Corput) and dimension 1 (Pascal matrix mod 2)
        // as 32-bit fixed-point fractions, in natural index order.
        inline uint32_t sobolDim0(uint32_t i) { return reverseBits32(i); }
        inline uint32_t sobolDim1(uint32_t i) {
            uint32_t r = 0;
            for (uint32_t v = 1u << 31; i; i >>= 1, v ^= v >> 1)
                if (i & 1u)
                    r ^= v;
            return r;
        }

        // Seeds for one (sub-stream, dimension) pair.
        struct DimSeeds {
                uint32_t index, x, y;
        };
        inline DimSeeds dimSeeds(uint64_t seedHash, uint32_t dim, uint32_t salt) {
            const uint64_t h1 = mix(seedHash, dim, salt);
            const uint64_t h2 = finalize64(h1);
            return DimSeeds{static_cast<uint32_t>(h1), static_cast<uint32_t>(h1 >> 32), static_cast<uint32_t>(h2)};
        }

        // Salts keep 1D and 2D draws of one dimension apart.
        inline constexpr uint32_t kSalt1D = 0x1D1D1D1Du;
        inline constexpr uint32_t kSalt2D = 0x2D2D2D2Du;

        inline float sobolGet1D(uint64_t seedHash, uint32_t dim, uint32_t index) {
            const DimSeeds s = dimSeeds(seedHash, dim, kSalt1D);
            const uint32_t idx = nestedUniformScramble(index, s.index);
            return toUnitFloat(nestedUniformScramble(sobolDim0(idx), s.x));
        }
        inline Sample2D sobolGet2D(uint64_t seedHash, uint32_t dim, uint32_t index) {
            const DimSeeds s = dimSeeds(seedHash, dim, kSalt2D);
            const uint32_t idx = nestedUniformScramble(index, s.index);
            return Sample2D{toUnitFloat(nestedUniformScramble(sobolDim0(idx), s.x)),
                            toUnitFloat(nestedUniformScramble(sobolDim1(idx), s.y))};
        }

        ///////////////////////////////////////////////////////////////////////
        // Independent
        ///////////////////////////////////////////////////////////////////////

        inline float independentGet1D(uint64_t seedHash, uint32_t dim, uint32_t index) {
            return toUnitFloat(static_cast<uint32_t>(mix(seedHash, dim, index, kSalt1D) >> 32));
        }
        inline Sample2D independentGet2D(uint64_t seedHash, uint32_t dim, uint32_t index) {
            const uint64_t h = mix(seedHash, dim, index, kSalt2D);
            return Sample2D{toUnitFloat(static_cast<uint32_t>(h >> 32)), toUnitFloat(static_cast<uint32_t>(finalize64(h) >> 32))};
        }

        ///////////////////////////////////////////////////////////////////////
        // PMJ02 (tables in samplingPmj02.cpp)
        ///////////////////////////////////////////////////////////////////////

        // Number of points per PMJ02 pattern; a draw at an index at or beyond it
        // falls back to scrambled Sobol (the documented index extension).
        inline constexpr uint32_t kPmj02Points = 4096;

        // Number of PMJ02 patterns.
        inline constexpr uint32_t kPmj02Patterns = 64;

        // Test-only: point `j` of pattern `pattern` exactly as built, before the
        // runtime index shuffle and Owen scramble.
        Sample2D pmj02Point(uint32_t pattern, uint32_t j);

        float pmj02Get1D(uint64_t seedHash, uint32_t dim, uint32_t index);
        Sample2D pmj02Get2D(uint64_t seedHash, uint32_t dim, uint32_t index);

    } // namespace detail

    ///////////////////////////////////////////////////////////////////////
    // Draws (pure: depend only on the backend and the arguments)
    ///////////////////////////////////////////////////////////////////////

    inline float get1D(Backend b, const SubstreamKey &k, uint32_t dim, uint32_t index) {
        switch (b) {
            case Backend::Sobol:
                return detail::sobolGet1D(k.seedHash, dim, index);
            case Backend::PMJ02:
                return detail::pmj02Get1D(k.seedHash, dim, index);
            case Backend::Independent:
            case Backend::Legacy: // never drawn from under Legacy; treated as Independent
            default:
                return detail::independentGet1D(k.seedHash, dim, index);
        }
    }

    inline Sample2D get2D(Backend b, const SubstreamKey &k, uint32_t dim, uint32_t index) {
        switch (b) {
            case Backend::Sobol:
                return detail::sobolGet2D(k.seedHash, dim, index);
            case Backend::PMJ02:
                return detail::pmj02Get2D(k.seedHash, dim, index);
            case Backend::Independent:
            case Backend::Legacy:
            default:
                return detail::independentGet2D(k.seedHash, dim, index);
        }
    }

    // Composed from consecutive slots of one sub-stream (FR-008).
    inline Sample3D get3D(Backend b, const SubstreamKey &k, uint32_t dim, uint32_t index) {
        const Sample2D a = get2D(b, k, dim, index);
        return Sample3D{a.u, a.v, get1D(b, k, dim + 1, index)};
    }
    inline Sample4D get4D(Backend b, const SubstreamKey &k, uint32_t dim, uint32_t index) {
        const Sample2D a = get2D(b, k, dim, index);
        const Sample2D c = get2D(b, k, dim + 1, index);
        return Sample4D{a.u, a.v, c.u, c.v};
    }

    ///////////////////////////////////////////////////////////////////////
    // Identity construction (pure)
    ///////////////////////////////////////////////////////////////////////

    // Division and remainder that round toward negative infinity: margin samples
    // have negative absolute coordinates at the image border.
    inline int32_t floorDiv(int32_t a, int32_t b) {
        int32_t q = a / b;
        if ((a % b != 0) && ((a < 0) != (b < 0)))
            --q;
        return q;
    }
    inline int32_t floorMod(int32_t a, int32_t b) {
        int32_t r = a % b;
        if (r != 0 && ((r < 0) != (b < 0)))
            r += b;
        return r;
    }

    inline SampleKey cameraKey(const SampleId &id) {
        uint64_t path = detail::mix(0xCA3E4Aull, static_cast<uint32_t>(id.px), static_cast<uint32_t>(id.py), id.frame, id.seed);
        SampleKey k;
        k.path = path;
        k.full = detail::mix(path, id.sampleIndex);
        k.pixelSampleIndex = static_cast<int32_t>(id.sampleIndex);
        k.kind = static_cast<uint8_t>(SampleKind::Camera);
        return k;
    }

    inline SampleKey cellKey(uint64_t stableId, float u, float v, uint32_t elementId, uint32_t frame, uint32_t seed) {
        SampleKey k;
        k.full = detail::mix(0xCE11ull, stableId, std::bit_cast<uint32_t>(u), std::bit_cast<uint32_t>(v),
                             (static_cast<uint64_t>(elementId) << 32) | frame);
        k.full = detail::mix(k.full, seed);
        k.path = k.full;
        k.pixelSampleIndex = -1;
        k.kind = static_cast<uint8_t>(SampleKind::Cell);
        return k;
    }

    inline SampleKey childKey(const SampleKey &parent, uint16_t depth, uint16_t callSite, uint32_t occurrence, uint32_t sampleIndex) {
        const uint64_t a = (static_cast<uint64_t>(depth) << 16) | callSite;
        const uint64_t b = (static_cast<uint64_t>(occurrence) << 32) | sampleIndex;
        SampleKey k;
        k.full = detail::mix(parent.full, a, b);
        k.path = detail::mix(parent.path, a, b);
        k.pixelSampleIndex = parent.pixelSampleIndex;
        k.kind = static_cast<uint8_t>(SampleKind::Child);
        return k;
    }

    // Pixel-sample points seed from `path` and offset the index by
    // pixelSampleIndex * samplesPerCall, so one pixel's draws at a call site form a
    // single stratified set across its camera samples. Cell points seed from `full`.
    inline SubstreamKey substream(const SampleKey &point, uint64_t shaderKey, uint16_t callSite, uint32_t occurrence, uint16_t depth, uint32_t samplesPerCall) {
        const bool pixelSample = point.pixelSampleIndex >= 0;
        SubstreamKey s;
        s.seedHash = detail::mix(pixelSample ? point.path : point.full, shaderKey, (static_cast<uint64_t>(depth) << 16) | callSite,
                                 occurrence);
        s.indexBase = pixelSample ? static_cast<uint32_t>(point.pixelSampleIndex) * samplesPerCall : 0u;
        return s;
    }

    ///////////////////////////////////////////////////////////////////////
    // Camera slots
    ///////////////////////////////////////////////////////////////////////

    // u/v for the jitter mapping; the draws are indexed by the camera sampleIndex
    // inside a sub-stream seeded by the pixel (its `path`).
    Sample2D cameraFilm(const CSamplingConfig &cfg, const SampleId &id);
    Sample2D cameraLens(const CSamplingConfig &cfg, const SampleId &id);
    float cameraTime(const CSamplingConfig &cfg, const SampleId &id);
    float cameraImportance(const CSamplingConfig &cfg, const SampleId &id);

    ///////////////////////////////////////////////////////////////////////
    // Class				:	CKeyedStream
    // Description			:	A local, non-sample stream for clustering, getDisp, the
    //							point hierarchy and the radiance cache: hash(key, counter++).
    //							The counter is local to one call frame; never store it.
    class CKeyedStream {
        public:
            explicit CKeyedStream(uint64_t k) : key(k), counter(0) {}

            uint32_t nextU32() { return static_cast<uint32_t>(detail::mix(key, counter++) >> 32); }
            float next1D() { return detail::toUnitFloat(nextU32()); }

        private:
            uint64_t key;
            uint64_t counter;
    };

    namespace detail {

        // Interleaves the low 16 bits of px (even bit positions) and py (odd bit
        // positions). The camera sequence supports 65536 pixels per axis: beyond that,
        // and for negative coordinates, pixels wrap modulo 65536 (the sequence index is
        // 64 bits, so this is the only wrap).
        uint32_t mortonIndex(int32_t px, int32_t py);

        // The index of this camera sample in its sequence. Sobol: the pixel's Morton
        // index times 2^k plus the sampleIndex, 2^k being the power-of-two ceiling of
        // nx * ny, so neighbouring pixels draw disjoint blocks of one 64-bit sequence.
        // Other backends: the sampleIndex.
        uint64_t cameraSequenceIndex(const CSamplingConfig &cfg, const SampleId &id);

        // Test-only: dimensions 0 and 1 of the Sobol sequence with scrambling off.
        Sample2D sobolUnscrambled(uint32_t index);

    } // namespace detail

} // namespace openrender::sampling

#endif
