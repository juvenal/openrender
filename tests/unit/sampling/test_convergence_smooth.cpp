// Sampling_ConvergenceSmooth (G9; SC-003 part a).
//
// Integrates f(x, y) = sin(pi x) sin(pi y) over the unit square, whose exact
// integral is 4/pi^2, with N = 2^k points (k = 2..12) from each backend, over
// 32 keys. The log-log slope of the RMSE against N is fitted by least squares.
#include <vector>

#include "sampling_test_util.h"

using namespace sptest;

namespace {

double fitSlope(sp::Backend be) {
    constexpr double kPi = 3.14159265358979323846;
    const double exact = 4.0 / (kPi * kPi);
    constexpr int kKeys = 32;
    std::vector<double> lx, ly;
    for (int k = 2; k <= 12; ++k) {
        const uint32_t n = 1u << k;
        double sumSq = 0.0;
        for (int key = 0; key < kKeys; ++key) {
            const sp::SubstreamKey sk = rawKey(0xFACEull + static_cast<uint64_t>(key) * 15485863u);
            double sum = 0.0;
            for (uint32_t i = 0; i < n; ++i) {
                const sp::Sample2D s = sp::get2D(be, sk, 0, i);
                sum += std::sin(kPi * s.u) * std::sin(kPi * s.v);
            }
            const double err = sum / n - exact;
            sumSq += err * err;
        }
        lx.push_back(std::log(static_cast<double>(n)));
        ly.push_back(std::log(std::sqrt(sumSq / kKeys)));
    }
    double mx = 0, my = 0;
    for (size_t i = 0; i < lx.size(); ++i) mx += lx[i] / lx.size(), my += ly[i] / lx.size();
    double sxy = 0, sxx = 0;
    for (size_t i = 0; i < lx.size(); ++i) sxy += (lx[i] - mx) * (ly[i] - my), sxx += (lx[i] - mx) * (lx[i] - mx);
    return sxy / sxx;
}

void convergenceSmooth() {
    const double sobol = fitSlope(sp::Backend::Sobol);
    const double pmj = fitSlope(sp::Backend::PMJ02);
    const double indep = fitSlope(sp::Backend::Independent);
    std::printf("log-log RMSE slope: Sobol %.3f  PMJ02 %.3f  Independent %.3f\n", sobol, pmj, indep);
    // The default backend (Sobol) and PMJ02 must reach -0.9; Independent is
    // expected near -0.5 and must reach -0.4.
    SP_CHECK(sobol <= -0.9);
    SP_CHECK(pmj <= -0.9);
    SP_CHECK(indep <= -0.4);
}

}  // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"convergence_smooth", convergenceSmooth}});
}
