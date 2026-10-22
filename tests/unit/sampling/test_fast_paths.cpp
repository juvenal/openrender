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

} // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"bit_reverse", bitReverse}, {"camera_seeds", cameraSeedsPerFrame}});
}
