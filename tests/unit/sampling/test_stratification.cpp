// Sampling_NetProperty (G3) and Sampling_PMJ02Property (G4).
//
// A (0, m, 2)-net in base 2 has exactly one point in every elementary interval
// [i/2^a, (i+1)/2^a) x [j/2^b, (j+1)/2^b) with a + b = m. The checks count the
// points in every interval of every shape, for every prefix size 2^m.
#include "sampling_test_util.h"

using namespace sptest;

namespace {

// Returns the number of (m, a) shapes whose prefix of 2^m points is not a net.
int netViolations(const std::vector<sp::Sample2D> &pts, int maxM) {
    int bad = 0;
    for (int m = 1; m <= maxM; ++m) {
        const uint32_t n = 1u << m;
        for (int a = 0; a <= m; ++a) {
            const int b = m - a;
            std::vector<uint8_t> count(n, 0);
            bool ok = true;
            for (uint32_t i = 0; i < n; ++i) {
                // Multiplying by a power of two is exact, and u, v < 1.
                const uint32_t x = static_cast<uint32_t>(std::floor(pts[i].u * static_cast<float>(1u << a)));
                const uint32_t y = static_cast<uint32_t>(std::floor(pts[i].v * static_cast<float>(1u << b)));
                if (x >= (1u << a) || y >= (1u << b)) {
                    ok = false;
                    break;
                }
                uint8_t &c = count[(x << b) | y];
                if (c) ok = false;
                ++c;
            }
            if (!ok) ++bad;
        }
    }
    return bad;
}

void checkNets(sp::Backend be, int maxM) {
    constexpr int kKeys = 8;
    constexpr uint32_t kDims = 64;
    const uint32_t n = 1u << maxM;
    int badTotal = 0, pairs = 0;
    for (int k = 0; k < kKeys; ++k) {
        const sp::SubstreamKey key = rawKey(0xC0FFEEull + static_cast<uint64_t>(k) * 7919u);
        for (uint32_t dim = 0; dim < kDims; ++dim) {
            std::vector<sp::Sample2D> pts(n);
            for (uint32_t i = 0; i < n; ++i) pts[i] = sp::get2D(be, key, dim, i);
            const int bad = netViolations(pts, maxM);
            if (bad && badTotal < 5)
                std::fprintf(stderr, "%s: key %d dim %u: %d shape(s) are not a (0,m,2)-net\n", backendName(be), k,
                             dim, bad);
            badTotal += bad;
            ++pairs;
        }
    }
    SP_CHECK(badTotal == 0);
    std::printf("%s: %d (key, dim) pairs checked, %d violations\n", backendName(be), pairs, badTotal);
}

void netProperty() { checkNets(sp::Backend::Sobol, 12); }

void pmj02Property() { checkNets(sp::Backend::PMJ02, 12); }

}  // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"net", netProperty}, {"pmj02", pmj02Property}});
}
