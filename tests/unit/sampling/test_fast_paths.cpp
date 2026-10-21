// Spec 039, Phase 11 (T122): the fast paths of the sampler library give the same bits as the
// plain forms they replace.
//
//   Sampling_BitReverse: detail::reverseBits32() (the hardware bit reverse, where the compiler
//     has __builtin_bitreverse32) against detail::reverseBits32Portable() (the shift-and-mask
//     form, the fallback for GCC 13) over every 16-bit pattern placed low and high, single bits,
//     runs, and a million LCG values.
//   Sampling_CameraSeedsPerFrame: cameraDraws() with the camera seeds computed once per frame
//     (cameraSeeds()) against the four per-slot functions (cameraFilm, cameraLens, cameraTime,
//     cameraImportance), bit for bit, for every backend, over a spread of pixels, sample
//     indices, frames, seeds and pixel sample counts.
//   Sampling_IndexScrambleCache (Phase 12, T130): the 64-bit index scramble with its high half
//     cached (indexScramble64Prefix) against nestedUniformScramble64(), for indices whose high
//     32 bits are the cached ones and for indices whose high bits differ (recomputed, never the
//     stale cached value).
//   Sampling_CameraSeedsHighIndex (Phase 12, T130): cameraDraws() with the frame's seeds and
//     cached prefixes against the per-slot functions at pixels far enough out that the camera
//     index has high bits (high 32 bits not zero), for every backend.
#include "sampling_test_util.h"

using namespace sptest;

namespace {

    void bitReverse() {
        std::printf("reverseBits32: %s\n", sp::detail::kHardwareBitReverse ? "hardware (__builtin_bitreverse32)" : "portable fallback");
        size_t checked = 0;
        auto check = [&](uint32_t x) {
            SP_CHECK(sp::detail::reverseBits32(x) == sp::detail::reverseBits32Portable(x));
            ++checked;
        };
        for (uint32_t v = 0; v < 65536u; ++v) {
            check(v);
            check(v << 16);
            check(v * 0x10001u);
        }
        for (int b = 0; b < 32; ++b) {
            check(1u << b);
            check(~(1u << b));
            check((b == 0) ? 0u : (~0u >> (32 - b)));
        }
        uint32_t s = 12345u;
        for (int k = 0; k < 1000000; ++k) {
            s = s * 1664525u + 1013904223u;
            check(s);
        }
        // The reference definition, bit by bit
        for (uint32_t x : {0u, 1u, 0x80000000u, 0x12345678u, 0xDEADBEEFu, 0xFFFFFFFFu}) {
            uint32_t r = 0;
            for (int b = 0; b < 32; ++b)
                r |= ((x >> b) & 1u) << (31 - b);
            SP_CHECK(sp::detail::reverseBits32(x) == r);
        }
        std::printf("%zu values compared\n", checked);
    }

    void cameraSeedsPerFrame() {
        size_t checked = 0;
        for (sp::Backend be : {sp::Backend::Sobol, sp::Backend::PMJ02, sp::Backend::Independent})
            for (int nxy : {1, 2, 3, 4, 8})
                for (uint32_t frame : {0u, 1u, 7u})
                    for (uint32_t seed : {0u, 99u, 4294967289u}) {
                        const sp::CSamplingConfig cfg{be, seed, frame, nxy, nxy, 1.0f};
                        const sp::CameraSeeds seeds = sp::cameraSeeds(cfg);
                        for (int32_t py = -3; py < 40; py += 7)
                            for (int32_t px = -5; px < 70; px += 9)
                                for (uint32_t s = 0; s < static_cast<uint32_t>(nxy * nxy); ++s)
                                    for (bool lens : {false, true}) {
                                        const sp::SampleId id{px, py, s, frame, seed};
                                        const sp::CameraDraws d = sp::cameraDraws(cfg, seeds, id, lens);
                                        SP_CHECK(bitsOf(d.film.u) == bitsOf(sp::cameraFilm(cfg, id).u));
                                        SP_CHECK(bitsOf(d.film.v) == bitsOf(sp::cameraFilm(cfg, id).v));
                                        SP_CHECK(bitsOf(d.time) == bitsOf(sp::cameraTime(cfg, id)));
                                        SP_CHECK(bitsOf(d.importance) == bitsOf(sp::cameraImportance(cfg, id)));
                                        if (lens) {
                                            SP_CHECK(bitsOf(d.lens.u) == bitsOf(sp::cameraLens(cfg, id).u));
                                            SP_CHECK(bitsOf(d.lens.v) == bitsOf(sp::cameraLens(cfg, id).v));
                                        }
                                        else
                                            SP_CHECK(d.lens.u == 0.0f && d.lens.v == 0.0f);
                                        ++checked;
                                    }
                    }
        std::printf("%zu camera samples compared\n", checked);
    }

    void indexScrambleCache() {
        size_t checked = 0, recomputed = 0;
        uint64_t s = 0x243F6A8885A308D3ull;
        auto next = [&]() {
            s = s * 6364136223846793005ull + 1442695040888963407ull;
            return s;
        };
        for (int seedCase = 0; seedCase < 64; ++seedCase) {
            const uint64_t seed = next();
            for (uint32_t cachedHi : {0u, 1u, 5u, 0xFFFFu, 0x80000000u, 0xFFFFFFFFu}) {
                const sp::detail::IndexScramble64Prefix prefix = sp::detail::indexScramble64Prefix(cachedHi, seed);
                for (int k = 0; k < 2000; ++k) {
                    const uint32_t lo = static_cast<uint32_t>(next() >> 32);
                    // The cached high half, a neighbouring one, and arbitrary ones
                    for (uint32_t hi : {cachedHi, cachedHi + 1u, cachedHi - 1u, static_cast<uint32_t>(next() >> 32)}) {
                        const uint64_t x = (static_cast<uint64_t>(hi) << 32) | lo;
                        SP_CHECK(sp::detail::nestedUniformScramble64Cached(x, seed, prefix) == sp::detail::nestedUniformScramble64(x, seed));
                        ++checked;
                        if (hi != cachedHi)
                            ++recomputed;
                    }
                }
            }
        }
        std::printf("%zu indices compared, %zu with a high half other than the cached one\n", checked, recomputed);
    }

    void cameraSeedsHighIndex() {
        size_t checked = 0, highIndices = 0;
        for (sp::Backend be : {sp::Backend::Sobol, sp::Backend::PMJ02, sp::Backend::Independent})
            for (int nxy : {1, 2, 4, 16})
                for (uint32_t seed : {0u, 99u}) {
                    const sp::CSamplingConfig cfg{be, seed, 3u, nxy, nxy, 1.0f};
                    const sp::CameraSeeds seeds = sp::cameraSeeds(cfg);
                    // Pixels from the frame's corner to the far end of the 65536-pixel range, and
                    // negative ones (the margin), so the Morton index crosses 2^32 >> k
                    for (int32_t py : {0, 1, 4095, 32768, 40000, 65535, -1, -40000})
                        for (int32_t px : {0, 3, 4095, 32767, 50000, 65534, 65535, -2, -65535})
                            for (uint32_t sIdx = 0; sIdx < static_cast<uint32_t>(nxy * nxy); sIdx += (nxy > 4 ? 37 : 1))
                                for (bool lens : {false, true}) {
                                    const sp::SampleId id{px, py, sIdx, cfg.frame, cfg.seed};
                                    if ((sp::detail::cameraSequenceIndex(cfg, id) >> 32) != 0)
                                        ++highIndices;
                                    const sp::CameraDraws d = sp::cameraDraws(cfg, seeds, id, lens);
                                    SP_CHECK(bitsOf(d.film.u) == bitsOf(sp::cameraFilm(cfg, id).u));
                                    SP_CHECK(bitsOf(d.film.v) == bitsOf(sp::cameraFilm(cfg, id).v));
                                    SP_CHECK(bitsOf(d.time) == bitsOf(sp::cameraTime(cfg, id)));
                                    SP_CHECK(bitsOf(d.importance) == bitsOf(sp::cameraImportance(cfg, id)));
                                    if (lens) {
                                        SP_CHECK(bitsOf(d.lens.u) == bitsOf(sp::cameraLens(cfg, id).u));
                                        SP_CHECK(bitsOf(d.lens.v) == bitsOf(sp::cameraLens(cfg, id).v));
                                    }
                                    ++checked;
                                }
                }
        std::printf("%zu camera samples compared, %zu with a 64-bit index (high bits set)\n", checked, highIndices);
        SP_CHECK(highIndices > 0); // the case the cache must not serve stale
    }

} // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv,
                   {{"bit_reverse", bitReverse},
                    {"camera_seeds", cameraSeedsPerFrame},
                    {"index_scramble_cache", indexScrambleCache},
                    {"camera_seeds_high_index", cameraSeedsHighIndex}});
}
