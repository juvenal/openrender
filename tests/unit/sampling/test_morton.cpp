// Sampling_MortonIndex and Sampling_CameraSobolBlocks (research R2.1).
//
// The Sobol camera sequence is indexed by mortonIndex(px, py) * 2^k +
// sampleIndex, 2^k being the power-of-two ceiling of the pixel's sample count.
// Neighbouring pixels then own disjoint, consecutive blocks of one sequence,
// and an aligned group of pixels draws one aligned block of it: a (0, m, 2)-net.
// The sequence index is 64 bits, so no two pixels share a block however high the
// sample count: with a 32-bit index, pixels whose Morton indices differ by a
// multiple of 2^(32-k) would collide. The only wrap left is the documented limit
// of 65536 pixels per axis (mortonIndex takes the low 16 bits of each).
#include <set>

#include "sampling_test_util.h"

using namespace sptest;

namespace {

    // Interleave written as a plain loop, as an independent reference.
    uint32_t referenceMorton(int32_t px, int32_t py) {
        uint32_t r = 0;
        for (int bit = 0; bit < 16; ++bit) {
            r |= ((static_cast<uint32_t>(px) >> bit) & 1u) << (2 * bit);
            r |= ((static_cast<uint32_t>(py) >> bit) & 1u) << (2 * bit + 1);
        }
        return r;
    }

    void mortonValues() {
        // Hand-computed: x bit i goes to bit 2i, y bit i to bit 2i + 1.
        struct Case {
                int32_t px, py;
                uint32_t want;
        };
        const Case hand[] = {
            {0, 0, 0u},
            {1, 0, 1u},
            {0, 1, 2u},
            {1, 1, 3u},
            {2, 0, 4u},
            {0, 2, 8u},
            {3, 3, 15u},
            {5, 3, 27u}, // x = 101b, y = 011b: 1 + 16 + 2 + 8
            {4, 4, 48u},
            {255, 0, 0x5555u},
            {0, 255, 0xAAAAu},
            {255, 255, 0xFFFFu},
            {65535, 0, 0x55555555u},
            {0, 65535, 0xAAAAAAAAu},
            {65535, 65535, 0xFFFFFFFFu},
            // Negative coordinates (margin samples at the image border, or a crop
            // window left of the origin) are taken modulo 65536.
            {-1, 0, 0x55555555u},
            {0, -1, 0xAAAAAAAAu},
            {-1, -1, 0xFFFFFFFFu},
            {-2, 0, 0x55555554u},
            {-65536, 0, 0u},
            {65536 + 3, 1, 7u},   // x = 3, y = 1: 1 + 4 + 2
            {-65536 + 5, 0, 17u}, // x = 5: 1 + 16
        };
        for (const Case &c : hand) {
            const uint32_t got = sp::detail::mortonIndex(c.px, c.py);
            if (got != c.want)
                std::fprintf(stderr, "mortonIndex(%d, %d) = %u, want %u\n", c.px, c.py, got, c.want);
            SP_CHECK(got == c.want);
        }

        // Against the loop reference, over a range that crosses zero and the 16-bit wrap.
        for (int32_t px = -70; px < 70; ++px)
            for (int32_t py = -70; py < 70; ++py) {
                SP_CHECK(sp::detail::mortonIndex(px, py) == referenceMorton(px, py));
                SP_CHECK(sp::detail::mortonIndex(px + 65536, py - 65536) == sp::detail::mortonIndex(px, py));
            }
        for (uint32_t i = 0; i < 4096; ++i) {
            const int32_t px = static_cast<int32_t>(splitmix(i) % 100000) - 50000, py = static_cast<int32_t>(splitmix(~i) % 100000) - 50000;
            SP_CHECK(sp::detail::mortonIndex(px, py) == referenceMorton(px, py));
        }

        // Locality: an aligned 2^s x 2^s group of pixels owns an aligned run of 4^s
        // consecutive Morton indices (negative groups included).
        for (int s = 1; s <= 4; ++s) {
            const int32_t side = 1 << s;
            for (int32_t gx = -3; gx <= 3; ++gx)
                for (int32_t gy = -3; gy <= 3; ++gy) {
                    std::set<uint32_t> seen;
                    for (int32_t dx = 0; dx < side; ++dx)
                        for (int32_t dy = 0; dy < side; ++dy)
                            seen.insert(sp::detail::mortonIndex(gx * side + dx, gy * side + dy));
                    SP_CHECK(seen.size() == static_cast<size_t>(side) * static_cast<size_t>(side));
                    SP_CHECK(*seen.rbegin() - *seen.begin() + 1 == seen.size()); // consecutive
                    SP_CHECK(*seen.begin() % seen.size() == 0);                  // aligned
                }
        }
    }

    // (0, m, 2)-net check on a point set of exactly 2^m points.
    bool isNet(const std::vector<sp::Sample2D> &pts, int m) {
        for (int a = 0; a <= m; ++a) {
            const int b = m - a;
            std::vector<uint8_t> count(pts.size(), 0);
            for (const auto &p : pts) {
                const uint32_t x = static_cast<uint32_t>(std::floor(p.u * static_cast<float>(1u << a)));
                const uint32_t y = static_cast<uint32_t>(std::floor(p.v * static_cast<float>(1u << b)));
                if (count[(x << b) | y]++)
                    return false;
            }
        }
        return true;
    }

    // 2^m strata of one dimension, each holding one value.
    bool isStratified(const std::vector<float> &v, int m) {
        std::vector<uint8_t> count(v.size(), 0);
        for (float f : v)
            if (count[static_cast<uint32_t>(std::floor(f * static_cast<float>(1u << m)))]++)
                return false;
        return true;
    }

    int ceilLog2(int n) {
        int k = 0;
        while ((1 << k) < n)
            ++k;
        return k;
    }

    void cameraSobolBlocks() {
        // Index formula and disjointness, for power-of-two and other sample counts.
        const std::pair<int, int> counts[] = {{1, 1}, {2, 2}, {3, 3}, {3, 4}, {8, 8}, {1, 2}};
        for (const auto &nxy : counts) {
            const int nspp = nxy.first * nxy.second, k = ceilLog2(nspp);
            sp::CSamplingConfig cfg{sp::Backend::Sobol, 7u, 2u, nxy.first, nxy.second, 1.0f};
            std::set<uint64_t> all;
            size_t expected = 0;
            for (int32_t py = -6; py < 10; ++py)
                for (int32_t px = -6; px < 10; ++px) {
                    uint64_t lo = ~0ull, hi = 0;
                    for (int s = 0; s < nspp; ++s) {
                        const sp::SampleId id{px, py, static_cast<uint32_t>(s), cfg.frame, cfg.seed};
                        const uint64_t idx = sp::detail::cameraSequenceIndex(cfg, id);
                        SP_CHECK(idx == (static_cast<uint64_t>(sp::detail::mortonIndex(px, py)) << k) + static_cast<uint64_t>(s));
                        all.insert(idx);
                        ++expected;
                        lo = std::min(lo, idx);
                        hi = std::max(hi, idx);
                    }
                    // Each pixel stays inside its own block of 2^k indices.
                    SP_CHECK((lo >> k) == (hi >> k) && (lo >> k) == sp::detail::mortonIndex(px, py));
                }
            if (all.size() != expected)
                std::fprintf(stderr, "%dx%d: %zu distinct camera indices for %zu samples\n", nxy.first, nxy.second,
                             all.size(), expected);
            SP_CHECK(all.size() == expected); // no two pixels draw the same index
        }

        // The other backends index by the sample alone (per-pixel sub-streams).
        for (sp::Backend be : {sp::Backend::PMJ02, sp::Backend::Independent}) {
            sp::CSamplingConfig cfg{be, 7u, 2u, 3, 3, 1.0f};
            for (uint32_t s = 0; s < 9; ++s)
                SP_CHECK(sp::detail::cameraSequenceIndex(cfg, sp::SampleId{-3, 12, s, 2u, 7u}) == s);
        }

        // Observable consequence: an aligned group of pixels draws one aligned
        // block of one sequence, so its camera samples form a (0, m, 2)-net, and
        // its time and importance samples fill 2^m strata. With one sequence per
        // pixel (no Morton ordering) a group of pixels is not stratified.
        struct Group {
                int nxy, side;
        };
        for (Group g : {Group{2, 2}, Group{2, 4}, Group{1, 8}, Group{4, 2}}) {
            const int nspp = g.nxy * g.nxy;
            const int m = ceilLog2(nspp * g.side * g.side);
            SP_CHECK((1 << m) == nspp * g.side * g.side);
            sp::CSamplingConfig cfg{sp::Backend::Sobol, 99u, 5u, g.nxy, g.nxy, 1.0f};
            for (int32_t gx : {-2, -1, 0, 1, 5})
                for (int32_t gy : {-1, 0, 2, 7}) {
                    std::vector<sp::Sample2D> film, lens;
                    std::vector<float> time, importance;
                    for (int dy = 0; dy < g.side; ++dy)
                        for (int dx = 0; dx < g.side; ++dx)
                            for (int s = 0; s < nspp; ++s) {
                                const sp::SampleId id{gx * g.side + dx, gy * g.side + dy, static_cast<uint32_t>(s), cfg.frame, cfg.seed};
                                film.push_back(sp::cameraFilm(cfg, id));
                                lens.push_back(sp::cameraLens(cfg, id));
                                time.push_back(sp::cameraTime(cfg, id));
                                importance.push_back(sp::cameraImportance(cfg, id));
                            }
                    SP_CHECK(isNet(film, m));
                    SP_CHECK(isNet(lens, m));
                    SP_CHECK(isStratified(time, m));
                    SP_CHECK(isStratified(importance, m));
                }
        }

        // A large frame at high sample counts. With a 32-bit index, two pixels whose
        // Morton indices differ by a multiple of 2^(32-k) get the same index (and
        // so the same samples). Here the pairs are picked on purpose to be such
        // collisions, and the 64-bit index must keep them apart.
        constexpr int32_t kWidth = 7680, kHeight = 4320; // 8K UHD
        for (int spp : {256, 4096}) {
            const int side = spp == 256 ? 16 : 64, k = ceilLog2(spp);
            sp::CSamplingConfig cfg{sp::Backend::Sobol, 3u, 1u, side, side, 1.0f};
            auto wrapped = [&](int32_t px, int32_t py) { // the old 32-bit index of sample 0
                return (static_cast<uint64_t>(sp::detail::mortonIndex(px, py)) << k) & 0xFFFFFFFFull;
            };

            // Deliberate pairs: Morton bit 32-k is x bit (32-k)/2, which for even
            // 32-k is the lowest x bit that falls off the end of a 32-bit index.
            const int xbit = (32 - k) / 2; // k = 8: 12, k = 12: 10
            const int32_t dx = 1 << xbit, dy = 1 << xbit;
            struct Pair {
                    int32_t ax, ay, bx, by;
            };
            const Pair pairs[] = {
                {100, 200, 100 + dx, 200},          // differ in x only
                {100, 200, 100, 200 + dy},          // differ in y only
                {5, 7, 5 + dx, 7 + dy},             // differ in both
                {7679 - dx, 4319 - dy, 7679, 4319}, // far corner
            };
            for (const Pair &p : pairs) {
                SP_CHECK(p.bx < kWidth && p.by < kHeight);
                SP_CHECK(sp::detail::mortonIndex(p.ax, p.ay) != sp::detail::mortonIndex(p.bx, p.by));
                SP_CHECK(wrapped(p.ax, p.ay) == wrapped(p.bx, p.by)); // a genuine 32-bit collision
                for (uint32_t s : {0u, 1u, static_cast<uint32_t>(spp - 1)}) {
                    const sp::SampleId a{p.ax, p.ay, s, cfg.frame, cfg.seed}, b{p.bx, p.by, s, cfg.frame, cfg.seed};
                    const uint64_t ia = sp::detail::cameraSequenceIndex(cfg, a), ib = sp::detail::cameraSequenceIndex(cfg, b);
                    SP_CHECK(ia != ib);
                    SP_CHECK(ia == (static_cast<uint64_t>(sp::detail::mortonIndex(p.ax, p.ay)) << k) + s);
                    SP_CHECK(ib == (static_cast<uint64_t>(sp::detail::mortonIndex(p.bx, p.by)) << k) + s);
                    // The colliding pixels draw different samples.
                    const sp::Sample2D fa = sp::cameraFilm(cfg, a), fb = sp::cameraFilm(cfg, b);
                    SP_CHECK(bitsOf(fa.u) != bitsOf(fb.u) || bitsOf(fa.v) != bitsOf(fb.v));
                }
            }

            // Every pixel of the frame that collides with pixel (100, 200) under a
            // 32-bit index has a different 64-bit index.
            int collisions = 0;
            const uint64_t target = wrapped(100, 200);
            for (int32_t py = 0; py < kHeight; ++py)
                for (int32_t px = 0; px < kWidth; ++px)
                    if (wrapped(px, py) == target && !(px == 100 && py == 200)) {
                        ++collisions;
                        const sp::SampleId a{100, 200, 0u, cfg.frame, cfg.seed}, b{px, py, 0u, cfg.frame, cfg.seed};
                        SP_CHECK(sp::detail::cameraSequenceIndex(cfg, a) != sp::detail::cameraSequenceIndex(cfg, b));
                    }
            std::printf("%d spp: %d pixels of the 8K frame collide with (100, 200) under a 32-bit index\n", spp, collisions);
            SP_CHECK(collisions > 0); // the scan is meaningful

            // A sparse lattice over the whole frame, two samples per pixel: all indices distinct.
            std::set<uint64_t> lattice;
            size_t count = 0;
            for (int32_t py = 0; py < kHeight; py += 13)
                for (int32_t px = 0; px < kWidth; px += 11)
                    for (uint32_t s : {0u, static_cast<uint32_t>(spp - 1)}) {
                        lattice.insert(sp::detail::cameraSequenceIndex(cfg, sp::SampleId{px, py, s, cfg.frame, cfg.seed}));
                        ++count;
                    }
            SP_CHECK(lattice.size() == count);
        }

        // Pure, and the sequence changes with the frame and the seed.
        sp::CSamplingConfig cfg{sp::Backend::Sobol, 7u, 2u, 2, 2, 1.0f};
        const sp::SampleId id{4, -3, 1u, 2u, 7u};
        SP_CHECK(bitsOf(sp::cameraFilm(cfg, id).u) == bitsOf(sp::cameraFilm(cfg, id).u));
        const sp::SampleId otherFrame{4, -3, 1u, 3u, 7u}, otherSeed{4, -3, 1u, 2u, 8u};
        SP_CHECK(bitsOf(sp::cameraFilm(cfg, otherFrame).u) != bitsOf(sp::cameraFilm(cfg, id).u));
        SP_CHECK(bitsOf(sp::cameraFilm(cfg, otherSeed).u) != bitsOf(sp::cameraFilm(cfg, id).u));
    }

} // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"morton", mortonValues}, {"camera_sobol_blocks", cameraSobolBlocks}});
}
