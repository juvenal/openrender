// Sampling_DrawIndependence (G7), Sampling_Padding (G8) and
// Sampling_Composite3D4D (FR-008).
#include <set>

#include "sampling_test_util.h"

using namespace sptest;

namespace {

sp::SampleKey pointKey() {
    return sp::cameraKey(sp::SampleId{12, -3, 5u, 2u, 77u});
}

// Four dimensions of eight draws each, from one sub-stream.
std::vector<uint32_t> record(sp::Backend be, const sp::SubstreamKey &sk) {
    std::vector<uint32_t> v;
    for (uint32_t dim = 0; dim < 4; ++dim)
        for (uint32_t i = 0; i < 8; ++i) {
            const sp::Sample2D s = sp::get2D(be, sk, dim, sk.indexBase + i);
            v.push_back(bitsOf(s.u));
            v.push_back(bitsOf(s.v));
        }
    return v;
}

void drawIndependence() {
    const sp::SampleKey pk = pointKey();
    const uint64_t shader = 0xABCDEFull;
    constexpr uint32_t kN = 8;

    for (sp::Backend be : kBackends) {
        // Draws at (callSite 3, occurrence 2), recorded before anything else.
        const std::vector<uint32_t> before = record(be, sp::substream(pk, shader, 3, 2, 0, kN));

        // Add draws at many other call sites and occurrences, in a different order.
        std::vector<std::vector<uint32_t>> others;
        for (uint16_t cs = 0; cs < 16; ++cs)
            for (uint32_t occ = 4; occ-- > 0;) {
                if (cs == 3 && occ == 2) continue;
                others.push_back(record(be, sp::substream(pk, shader, cs, occ, 0, kN)));
            }

        const std::vector<uint32_t> after = record(be, sp::substream(pk, shader, 3, 2, 0, kN));
        SP_CHECK(before == after);

        // Different (callSite, occurrence) pairs give different values.
        for (const auto &o : others) SP_CHECK(o != before);
    }

    // Sub-streams with different call sites, occurrences, depths or shaders have
    // distinct seedHash.
    std::set<uint64_t> seeds;
    size_t count = 0;
    for (uint16_t cs = 0; cs < 64; ++cs)
        for (uint32_t occ = 0; occ < 64; ++occ)
            for (uint16_t depth = 0; depth < 3; ++depth)
                for (uint64_t shader2 : {1ull, 2ull}) {
                    seeds.insert(sp::substream(pk, shader2, cs, occ, depth, kN).seedHash);
                    ++count;
                }
    SP_CHECK(seeds.size() == count);
}

void padding() {
    for (sp::Backend be : kBackends) {
        const sp::SubstreamKey key = rawKey(99);
        // Far beyond any table: dim up to 2^20, index up to 2^24.
        for (uint32_t dim : {0u, 63u, 64u, 1000u, 1u << 16, (1u << 20) - 1u, 1u << 20})
            for (uint32_t index : {0u, 1u, 4095u, 4096u, 1u << 20, (1u << 24) - 1u, 1u << 24}) {
                const sp::Sample2D s = sp::get2D(be, key, dim, index);
                SP_CHECK(inRange(s.u) && inRange(s.v));
                SP_CHECK(inRange(sp::get1D(be, key, dim, index)));
            }

        // No repeated 2D tuple across 1024 dimensions for a fixed key (spot check at
        // two indices).
        for (uint32_t index : {0u, 5u}) {
            std::set<std::pair<uint32_t, uint32_t>> seen;
            for (uint32_t dim = 0; dim < 1024; ++dim) {
                const sp::Sample2D s = sp::get2D(be, key, dim, index);
                seen.insert({bitsOf(s.u), bitsOf(s.v)});
            }
            if (seen.size() != 1024)
                std::fprintf(stderr, "%s: index %u: only %zu distinct tuples in 1024 dimensions\n", backendName(be),
                             index, seen.size());
            SP_CHECK(seen.size() == 1024);
        }
    }
}

void composite3D4D() {
    for (sp::Backend be : kBackends) {
        const sp::SubstreamKey key = rawKey(2024);
        for (uint32_t dim : {0u, 1u, 7u, 100u})
            for (uint32_t i : {0u, 1u, 2u, 31u, 4097u}) {
                const sp::Sample2D a = sp::get2D(be, key, dim, i);
                const float b = sp::get1D(be, key, dim + 1, i);
                const sp::Sample2D c = sp::get2D(be, key, dim + 1, i);

                const sp::Sample3D s3 = sp::get3D(be, key, dim, i);
                SP_CHECK(s3.u == a.u && s3.v == a.v && s3.w == b);
                SP_CHECK(inRange(s3.u) && inRange(s3.v) && inRange(s3.w));

                const sp::Sample4D s4 = sp::get4D(be, key, dim, i);
                SP_CHECK(s4.u == a.u && s4.v == a.v && s4.w == c.u && s4.x == c.v);
                SP_CHECK(inRange(s4.u) && inRange(s4.v) && inRange(s4.w) && inRange(s4.x));

                // Deterministic: a second call gives the same values.
                const sp::Sample3D t3 = sp::get3D(be, key, dim, i);
                const sp::Sample4D t4 = sp::get4D(be, key, dim, i);
                SP_CHECK(t3.u == s3.u && t3.v == s3.v && t3.w == s3.w);
                SP_CHECK(t4.u == s4.u && t4.v == s4.v && t4.w == s4.w && t4.x == s4.x);
            }
    }
}

}  // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv,
                   {{"draw_independence", drawIndependence}, {"padding", padding}, {"composite", composite3D4D}});
}
