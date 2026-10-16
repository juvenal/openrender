// Sampling_Uniformity and Sampling_Discrepancy (G6).
#include <algorithm>
#include <numeric>

#include "sampling_test_util.h"

using namespace sptest;

namespace {

// The test is one family of kBackends x kDims = 48 chi-square checks, each
// run once on a fixed key. A fixed per-check level of p >= 0.01 would fail a
// correct generator about 1 time in 100 per check (about 38% over 48 checks,
// and for a deterministic hash that is a property of the key, not of the
// code). The checks are therefore Bonferroni-corrected: each must reach
// p >= 0.01 / 48, which holds the false-failure chance of the whole test at
// about 1% for any correct generator, while a biased one (p near 0) still
// fails every dimension it affects.
void uniformity() {
    constexpr uint32_t kDraws = 1u << 16;
    constexpr int kBins = 64;
    constexpr uint32_t kDims = 16;
    constexpr double kFamilyLevel = 0.01;
    const double perCheckLevel = kFamilyLevel / (static_cast<double>(sizeof(kBackends) / sizeof(kBackends[0])) * kDims);
    for (sp::Backend be : kBackends) {
        const sp::SubstreamKey key = rawKey(0x5EEDull);
        double minP = 1.0;
        for (uint32_t dim = 0; dim < kDims; ++dim) {
            std::vector<uint32_t> bins(kBins, 0);
            for (uint32_t i = 0; i < kDraws; ++i) {
                const float v = sp::get1D(be, key, dim, i);
                const int b = static_cast<int>(v * static_cast<float>(kBins));
                ++bins[std::min(b, kBins - 1)];
            }
            const double expected = static_cast<double>(kDraws) / kBins;
            double chi2 = 0.0;
            for (uint32_t c : bins) chi2 += (c - expected) * (c - expected) / expected;
            const double p = chiSquareP(chi2, kBins - 1);
            minP = std::min(minP, p);
            if (!(p >= perCheckLevel))
                std::fprintf(stderr, "%s: dim %u: chi2 %.2f p %.4g < %.4g\n", backendName(be), dim, chi2, p,
                             perCheckLevel);
            SP_CHECK(p >= perCheckLevel);
        }
        std::printf("%s: min p over %u dimensions = %.4g (per-check level %.3g)\n", backendName(be), kDims, minP,
                    perCheckLevel);
    }
}

// Exact 2D star discrepancy: the largest |count/N - area| over boxes
// [0,x) x [0,y). The extremes are reached with the corner on a point
// coordinate or on 1 (Dobkin and Eppstein), counting the points strictly
// inside the box and inside its closure.
double starDiscrepancy(std::vector<sp::Sample2D> pts) {
    const size_t n = pts.size();
    std::sort(pts.begin(), pts.end(), [](const sp::Sample2D &a, const sp::Sample2D &b) { return a.u < b.u; });
    std::vector<float> ysCorner;
    for (const auto &p : pts) ysCorner.push_back(p.v);
    ysCorner.push_back(1.0f);

    double worst = 0.0;
    std::vector<float> open, closed;  // sorted v of points with u < x, and u <= x
    std::vector<float> xs;
    for (const auto &p : pts) xs.push_back(p.u);
    xs.push_back(1.0f);
    for (size_t ix = 0; ix < xs.size(); ++ix) {
        const float x = xs[ix];
        // open holds the points with u < x, closed those with u <= x.
        open.clear();
        closed.clear();
        for (const auto &p : pts) {
            if (p.u < x) open.push_back(p.v);
            if (p.u <= x) closed.push_back(p.v);
        }
        std::sort(open.begin(), open.end());
        std::sort(closed.begin(), closed.end());
        for (float y : ysCorner) {
            const double area = static_cast<double>(x) * static_cast<double>(y);
            const double nOpen = static_cast<double>(std::lower_bound(open.begin(), open.end(), y) - open.begin());
            const double nClosed = static_cast<double>(std::upper_bound(closed.begin(), closed.end(), y) - closed.begin());
            worst = std::max(worst, area - nOpen / static_cast<double>(n));
            worst = std::max(worst, nClosed / static_cast<double>(n) - area);
        }
    }
    return worst;
}

void discrepancy() {
    constexpr int kKeys = 8;
    for (uint32_t n : {64u, 256u, 1024u}) {
        double mean[3] = {0, 0, 0};
        for (int be = 0; be < 3; ++be) {
            for (int k = 0; k < kKeys; ++k) {
                const sp::SubstreamKey key = rawKey(0xD15C0ull + static_cast<uint64_t>(k) * 104729u);
                std::vector<sp::Sample2D> pts(n);
                for (uint32_t i = 0; i < n; ++i) pts[i] = sp::get2D(kBackends[be], key, 0, i);
                mean[be] += starDiscrepancy(pts) / kKeys;
            }
        }
        std::printf("N=%u: star discrepancy (mean of %d keys)  Sobol %.5f  PMJ02 %.5f  Independent %.5f\n", n, kKeys,
                    mean[0], mean[1], mean[2]);
        SP_CHECK(mean[0] < mean[2]);  // Sobol below Independent
        SP_CHECK(mean[1] < mean[2]);  // PMJ02 below Independent
    }
}

}  // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"uniformity", uniformity}, {"discrepancy", discrepancy}});
}
