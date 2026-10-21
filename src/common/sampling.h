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
        Independent
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

    // Immutable per frame; built by the renderer core. originX/originY are the rendered
    // window's top-left pixel in the full image (the crop window's origin, 0 without a
    // crop): the hiders count their buckets from the window, and add it to make a
    // subpixel absolute (spec 039, T116). screenLeft/screenTop are the full image's
    // screen-window origin, from which the raytrace hider projects an absolute raster
    // position, so a crop projects its pixels exactly as the full frame does (T117).
    struct CSamplingConfig {
            Backend backend;
            uint32_t seed, frame;
            int nx, ny;
            float jitter;
            int originX = 0, originY = 0;
            float screenLeft = 0.0f, screenTop = 0.0f;
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

        // One finalizer round instead of mix()'s two: for the per-call and per-draw keys of the
        // shading draws, which run once per call and once per draw (spec 039, T123). The value
        // is scattered by a golden-ratio multiply before it meets the hash, so neighbouring
        // values (call sites, occurrences, dimensions, indices) land far apart.
        inline uint64_t mixFast(uint64_t h, uint64_t v) { return finalize64(h ^ (v * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull)); }

        // The top 24 bits of a 32-bit value as a float in [0, 1 - 2^-24] (never 1.0).
        inline float toUnitFloat(uint32_t bits) {
            return static_cast<float>(bits >> 8) * 0x1p-24f;
        }

        ///////////////////////////////////////////////////////////////////////
        // Owen-scrambled (0,2) machinery (Burley 2020)
        ///////////////////////////////////////////////////////////////////////

        // The portable bit reversal (shift and mask). reverseBits32() uses the hardware
        // instruction where the compiler has __builtin_bitreverse32 (Clang; one `rbit` on
        // ARM64) and this form elsewhere (GCC 13): the same bits either way
        // (Sampling_BitReverse, spec 039 T122).
        inline uint32_t reverseBits32Portable(uint32_t x) {
            x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
            x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
            x = ((x >> 4) & 0x0F0F0F0Fu) | ((x & 0x0F0F0F0Fu) << 4);
            x = ((x >> 8) & 0x00FF00FFu) | ((x & 0x00FF00FFu) << 8);
            return (x >> 16) | (x << 16);
        }
#if defined(__has_builtin)
#if __has_builtin(__builtin_bitreverse32)
#define OPENRENDER_SAMPLING_HW_BITREVERSE 1
#endif
#endif
#ifdef OPENRENDER_SAMPLING_HW_BITREVERSE
        inline constexpr bool kHardwareBitReverse = true;
        inline uint32_t reverseBits32(uint32_t x) { return __builtin_bitreverse32(x); }
#else
        inline constexpr bool kHardwareBitReverse = false;
        inline uint32_t reverseBits32(uint32_t x) { return reverseBits32Portable(x); }
#endif
#undef OPENRENDER_SAMPLING_HW_BITREVERSE

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
        // The shading draws' seeds (spec 039, T123): one finalizer round from the call's key, the
        // third seed from a multiply of the first round's value. The camera keeps dimSeeds()
        // (its seeds are computed once per frame, so their cost does not matter).
        inline DimSeeds dimSeedsFast(uint64_t seedHash, uint32_t dim, uint32_t salt) {
            const uint64_t h1 = mixFast(seedHash, (static_cast<uint64_t>(salt) << 32) | dim);
            const uint64_t h2 = (h1 ^ (h1 >> 29)) * 0xD1B54A32D192ED03ull;
            return DimSeeds{static_cast<uint32_t>(h1), static_cast<uint32_t>(h1 >> 32), static_cast<uint32_t>(h2 >> 32)};
        }

        // Salts keep 1D and 2D draws of one dimension apart.
        inline constexpr uint32_t kSalt1D = 0x1D1D1D1Du;
        inline constexpr uint32_t kSalt2D = 0x2D2D2D2Du;

        // A draw from seeds already computed: a caller drawing many samples of one
        // (sub-stream, dimension) keeps the seeds (spec 039, T114).
        inline float sobolGet1DSeeded(const DimSeeds &s, uint32_t index) {
            const uint32_t idx = nestedUniformScramble(index, s.index);
            return toUnitFloat(nestedUniformScramble(sobolDim0(idx), s.x));
        }
        inline Sample2D sobolGet2DSeeded(const DimSeeds &s, uint32_t index) {
            const uint32_t idx = nestedUniformScramble(index, s.index);
            return Sample2D{toUnitFloat(nestedUniformScramble(sobolDim0(idx), s.x)),
                            toUnitFloat(nestedUniformScramble(sobolDim1(idx), s.y))};
        }
        inline float sobolGet1D(uint64_t seedHash, uint32_t dim, uint32_t index) { return sobolGet1DSeeded(dimSeedsFast(seedHash, dim, kSalt1D), index); }
        inline Sample2D sobolGet2D(uint64_t seedHash, uint32_t dim, uint32_t index) {
            return sobolGet2DSeeded(dimSeedsFast(seedHash, dim, kSalt2D), index);
        }

        // 64-bit sample indices (the Morton-ordered camera sequence). The index is
        // scrambled by a nested uniform permutation of 64 bits, built from two 32-bit
        // ones: the high half is permuted by its own seed, the low half by a seed that
        // also depends on the whole high half, so every bit still flips as a function
        // of the bits above it only (aligned blocks of 2^m indices stay aligned blocks).
        inline uint64_t nestedUniformScramble64(uint64_t x, uint64_t seed) {
            const uint32_t hi = static_cast<uint32_t>(x >> 32), lo = static_cast<uint32_t>(x);
            const uint32_t hiOut = nestedUniformScramble(hi, static_cast<uint32_t>(seed >> 32));
            const uint32_t loOut = nestedUniformScramble(lo, static_cast<uint32_t>(seed) ^ static_cast<uint32_t>(finalize64(hi)));
            return (static_cast<uint64_t>(hiOut) << 32) | loOut;
        }

        // The part of nestedUniformScramble64() that depends only on the index's high half and
        // the seed (spec 039, Phase 12 T130): the permuted high half and the low half's seed.
        // The camera keeps it per frame for a high half of 0, the case of every index below
        // 2^32; an index with any other high half is scrambled in full, never from the cache.
        struct IndexScramble64Prefix {
                uint32_t hi = 0, hiOut = 0, loSeed = 0;
        };
        inline IndexScramble64Prefix indexScramble64Prefix(uint32_t hi, uint64_t seed) {
            return IndexScramble64Prefix{hi, nestedUniformScramble(hi, static_cast<uint32_t>(seed >> 32)),
                                         static_cast<uint32_t>(seed) ^ static_cast<uint32_t>(finalize64(hi))};
        }
        // nestedUniformScramble64(x, seed), bit for bit, given the prefix of seed for some high half.
        inline uint64_t nestedUniformScramble64Cached(uint64_t x, uint64_t seed, const IndexScramble64Prefix &prefix) {
            if (static_cast<uint32_t>(x >> 32) != prefix.hi)
                return nestedUniformScramble64(x, seed);
            return (static_cast<uint64_t>(prefix.hiOut) << 32) | nestedUniformScramble(static_cast<uint32_t>(x), prefix.loSeed);
        }

        // The first 32 digits of Sobol dimensions 0 and 1 for a 64-bit index.
        inline uint32_t sobolDim0Of64(uint64_t i) {
            return reverseBits32(static_cast<uint32_t>(i)); // the top 32 digits of the 64-bit van der Corput value
        }
        // Dimension 1's generator matrix applied to a 64-bit index a byte at a time: entry
        // [b][x] is the XOR of the columns of the bits set in byte value x at byte b (the
        // top 32 digits of each column). The same bits as the bit-at-a-time loop
        // (v = 2^63, v ^= v >> 1 per bit), in 8 lookups instead of up to 64 iterations:
        // the camera index is a Morton code shifted up, so it has many bits (spec 039, T112).
        struct SobolDim1Of64Table {
                uint32_t entry[8][256];
        };
        constexpr SobolDim1Of64Table makeSobolDim1Of64Table() {
            SobolDim1Of64Table t{};
            uint64_t column[64] = {};
            uint64_t v = 1ull << 63;
            for (int k = 0; k < 64; k++, v ^= v >> 1)
                column[k] = v;
            for (int b = 0; b < 8; b++)
                for (int x = 0; x < 256; x++) {
                    uint64_t r = 0;
                    for (int j = 0; j < 8; j++)
                        if (x & (1 << j))
                            r ^= column[8 * b + j];
                    t.entry[b][x] = static_cast<uint32_t>(r >> 32);
                }
            return t;
        }
        inline constexpr SobolDim1Of64Table kSobolDim1Of64 = makeSobolDim1Of64Table();
        inline uint32_t sobolDim1Of64(uint64_t i) {
            uint32_t r = 0;
            for (int b = 0; i; b++, i >>= 8)
                r ^= kSobolDim1Of64.entry[b][i & 0xFFu];
            return r;
        }

        // From seeds already computed (the camera keeps them per frame, spec 039 T122).
        inline float sobolGet1D64Seeded(const DimSeeds &s, uint64_t index) {
            const uint64_t idx = nestedUniformScramble64(index, (static_cast<uint64_t>(s.x) << 32) | s.index);
            return toUnitFloat(nestedUniformScramble(sobolDim0Of64(idx), s.x));
        }
        inline Sample2D sobolGet2D64Seeded(const DimSeeds &s, uint64_t index) {
            const uint64_t idx = nestedUniformScramble64(index, (static_cast<uint64_t>(s.x) << 32) | s.index);
            return Sample2D{toUnitFloat(nestedUniformScramble(sobolDim0Of64(idx), s.x)),
                            toUnitFloat(nestedUniformScramble(sobolDim1Of64(idx), s.y))};
        }
        // From seeds and the index scramble's prefix already computed (the camera keeps both per
        // frame, spec 039 Phase 12 T130). The prefix must be indexScramble64Prefix(hi, indexSeed64(s)).
        inline uint64_t indexSeed64(const DimSeeds &s) { return (static_cast<uint64_t>(s.x) << 32) | s.index; }
        inline float sobolGet1D64Cached(const DimSeeds &s, const IndexScramble64Prefix &p, uint64_t index) {
            const uint64_t idx = nestedUniformScramble64Cached(index, indexSeed64(s), p);
            return toUnitFloat(nestedUniformScramble(sobolDim0Of64(idx), s.x));
        }
        inline Sample2D sobolGet2D64Cached(const DimSeeds &s, const IndexScramble64Prefix &p, uint64_t index) {
            const uint64_t idx = nestedUniformScramble64Cached(index, indexSeed64(s), p);
            return Sample2D{toUnitFloat(nestedUniformScramble(sobolDim0Of64(idx), s.x)),
                            toUnitFloat(nestedUniformScramble(sobolDim1Of64(idx), s.y))};
        }
        inline float sobolGet1D64(uint64_t seedHash, uint32_t dim, uint64_t index) { return sobolGet1D64Seeded(dimSeeds(seedHash, dim, kSalt1D), index); }
        inline Sample2D sobolGet2D64(uint64_t seedHash, uint32_t dim, uint64_t index) {
            return sobolGet2D64Seeded(dimSeeds(seedHash, dim, kSalt2D), index);
        }

        ///////////////////////////////////////////////////////////////////////
        // Independent
        ///////////////////////////////////////////////////////////////////////

        // One finalizer round per draw: the key of (dimension, salt) and the sample index in one
        // 64-bit value (spec 039, T123).
        inline float independentGet1D(uint64_t seedHash, uint32_t dim, uint32_t index) {
            return toUnitFloat(static_cast<uint32_t>(mixFast(seedHash, (static_cast<uint64_t>(dim ^ kSalt1D) << 32) | index) >> 32));
        }
        inline Sample2D independentGet2D(uint64_t seedHash, uint32_t dim, uint32_t index) {
            const uint64_t h = mixFast(seedHash, (static_cast<uint64_t>(dim ^ kSalt2D) << 32) | index);
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
    // Names (Option "sampling" "sampler")
    ///////////////////////////////////////////////////////////////////////

    // The option name of a backend: "sobol", "pmj02" or "independent".
    const char *samplerName(Backend b);

    // Looks a name up among those three ("legacy", internal before Phase 9, is unknown).
    // Returns false (leaving `out` alone) for an unknown name.
    bool samplerFromName(const char *name, Backend &out);

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
    // substream() in two steps, the same value: the part that depends only on the point and
    // the shader, which a caller can keep for every call of one shader at one point, and the
    // rest (spec 039, T114).
    inline uint64_t substreamPartial(const SampleKey &point, uint64_t shaderKey) {
        return detail::mix(point.pixelSampleIndex >= 0 ? point.path : point.full, shaderKey);
    }
    inline SubstreamKey substreamFromPartial(uint64_t partial, const SampleKey &point, uint16_t callSite, uint32_t occurrence, uint16_t depth,
                                             uint32_t samplesPerCall) {
        const bool pixelSample = point.pixelSampleIndex >= 0;
        SubstreamKey s;
        // One hash per call: the call (depth, call site) and its occurrence in one 64-bit value
        // (spec 039, T123)
        s.seedHash = detail::mixFast(partial, (((static_cast<uint64_t>(depth) << 16) | callSite) << 32) | occurrence);
        s.indexBase = pixelSample ? static_cast<uint32_t>(point.pixelSampleIndex) * samplesPerCall : 0u;
        return s;
    }
    inline SubstreamKey substream(const SampleKey &point, uint64_t shaderKey, uint16_t callSite, uint32_t occurrence, uint16_t depth, uint32_t samplesPerCall) {
        return substreamFromPartial(substreamPartial(point, shaderKey), point, callSite, occurrence, depth, samplesPerCall);
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

    // All the camera slots of one sample at once, the same values as the four functions
    // above: the sequence seed and index are computed once per sample instead of once per
    // slot (spec 039, T112). `lens` is drawn only when wantLens (else 0, 0). cameraDraws()
    // is defined inline after the detail helpers below.
    struct CameraDraws {
            Sample2D film, lens;
            float time, importance;
    };

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
        inline uint32_t mortonIndex(int32_t px, int32_t py) {
            // Spreads the low 16 bits of v to the even bit positions.
            auto spread = [](uint32_t v) {
                v &= 0xFFFFu;
                v = (v | (v << 8)) & 0x00FF00FFu;
                v = (v | (v << 4)) & 0x0F0F0F0Fu;
                v = (v | (v << 2)) & 0x33333333u;
                v = (v | (v << 1)) & 0x55555555u;
                return v;
            };
            return spread(static_cast<uint32_t>(px)) | (spread(static_cast<uint32_t>(py)) << 1);
        }

        // Smallest k with 2^k >= n.
        inline unsigned ceilLog2(int n) {
            unsigned k = 0;
            while ((1ll << k) < n)
                ++k;
            return k;
        }

        // The Sobol camera sequence is one sequence for the whole frame, seeded by the
        // frame and the seed only: a pixel is a block of the sequence, not a sequence
        // of its own (research R2.1). The other backends keep one sub-stream per pixel,
        // seeded by the pixel's path and indexed by the sample's position in the pixel.
        inline constexpr uint64_t kCameraSequenceSalt = 0xCA3E5ull;

        // The index of this camera sample in its sequence. Sobol: the pixel's Morton
        // index times 2^k plus the sampleIndex, 2^k being the power-of-two ceiling of
        // nx * ny, so neighbouring pixels draw disjoint blocks of one 64-bit sequence.
        // Other backends: the sampleIndex.
        inline uint64_t cameraSequenceIndex(const CSamplingConfig &cfg, const SampleId &id) {
            if (cfg.backend != Backend::Sobol)
                return id.sampleIndex;
            return (static_cast<uint64_t>(mortonIndex(id.px, id.py)) << ceilLog2(cfg.nx * cfg.ny)) + id.sampleIndex;
        }

        // Test-only: dimensions 0 and 1 of the Sobol sequence with scrambling off.
        Sample2D sobolUnscrambled(uint32_t index);

    } // namespace detail

    // The camera's per-dimension seeds of a frame (spec 039, T122). Under Sobol the camera is
    // one sequence per frame, so its four slots' seeds depend only on the frame and the seed:
    // computed once per frame (the shading context keeps them) instead of once per subpixel.
    // The other backends seed per pixel and keep nothing here. Valid for the samples whose
    // SampleId carries cfg.frame and cfg.seed (every camera sample of the frame).
    // Each slot also keeps its index scramble's prefix for a high half of 0 (Phase 12, T130):
    // read-only once the frame is set up, so the threads share it without synchronisation.
    struct CameraSeeds {
            bool sequence = false;
            detail::DimSeeds film{}, lens{}, time{}, importance{};
            detail::IndexScramble64Prefix filmPrefix{}, lensPrefix{}, timePrefix{}, importancePrefix{};
    };
    inline CameraSeeds cameraSeeds(const CSamplingConfig &cfg) {
        CameraSeeds s;
        if (cfg.backend == Backend::Sobol) {
            const uint64_t seed = detail::mix(detail::kCameraSequenceSalt, cfg.frame, cfg.seed);
            s.sequence = true;
            s.film = detail::dimSeeds(seed, Film2D, detail::kSalt2D);
            s.lens = detail::dimSeeds(seed, Lens2D, detail::kSalt2D);
            s.time = detail::dimSeeds(seed, Time1D, detail::kSalt1D);
            s.importance = detail::dimSeeds(seed, Importance1D, detail::kSalt1D);
            s.filmPrefix = detail::indexScramble64Prefix(0u, detail::indexSeed64(s.film));
            s.lensPrefix = detail::indexScramble64Prefix(0u, detail::indexSeed64(s.lens));
            s.timePrefix = detail::indexScramble64Prefix(0u, detail::indexSeed64(s.time));
            s.importancePrefix = detail::indexScramble64Prefix(0u, detail::indexSeed64(s.importance));
        }
        return s;
    }

    // All the camera slots of one sample with the frame's seeds (the same values as
    // cameraDraws(cfg, id, wantLens) below).
    inline CameraDraws cameraDraws(const CSamplingConfig &cfg, const CameraSeeds &seeds, const SampleId &id, bool wantLens) {
        CameraDraws d;
        d.lens = Sample2D{0.0f, 0.0f};
        if (seeds.sequence) {
            const uint64_t index = detail::cameraSequenceIndex(cfg, id);
            d.film = detail::sobolGet2D64Cached(seeds.film, seeds.filmPrefix, index);
            if (wantLens)
                d.lens = detail::sobolGet2D64Cached(seeds.lens, seeds.lensPrefix, index);
            d.time = detail::sobolGet1D64Cached(seeds.time, seeds.timePrefix, index);
            d.importance = detail::sobolGet1D64Cached(seeds.importance, seeds.importancePrefix, index);
        }
        else {
            const SubstreamKey sub{cameraKey(id).path, 0u};
            d.film = get2D(cfg.backend, sub, Film2D, id.sampleIndex);
            if (wantLens)
                d.lens = get2D(cfg.backend, sub, Lens2D, id.sampleIndex);
            d.time = get1D(cfg.backend, sub, Time1D, id.sampleIndex);
            d.importance = get1D(cfg.backend, sub, Importance1D, id.sampleIndex);
        }
        return d;
    }

    // All the camera slots of one sample (CameraDraws, above). Header-inline, so the
    // hiders draw their camera samples without a call into libRIBCommon (spec 039, T113).
    inline CameraDraws cameraDraws(const CSamplingConfig &cfg, const SampleId &id, bool wantLens) {
        CameraDraws d;
        d.lens = Sample2D{0.0f, 0.0f};
        if (cfg.backend == Backend::Sobol) {
            const uint64_t seed = detail::mix(detail::kCameraSequenceSalt, id.frame, id.seed);
            const uint64_t index = detail::cameraSequenceIndex(cfg, id);
            d.film = detail::sobolGet2D64(seed, Film2D, index);
            if (wantLens)
                d.lens = detail::sobolGet2D64(seed, Lens2D, index);
            d.time = detail::sobolGet1D64(seed, Time1D, index);
            d.importance = detail::sobolGet1D64(seed, Importance1D, index);
        }
        else {
            const SubstreamKey sub{cameraKey(id).path, 0u};
            d.film = get2D(cfg.backend, sub, Film2D, id.sampleIndex);
            if (wantLens)
                d.lens = get2D(cfg.backend, sub, Lens2D, id.sampleIndex);
            d.time = get1D(cfg.backend, sub, Time1D, id.sampleIndex);
            d.importance = get1D(cfg.backend, sub, Importance1D, id.sampleIndex);
        }
        return d;
    }

} // namespace openrender::sampling

#endif
