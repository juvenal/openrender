// Sampling_SobolReference (G5): with scrambling off, dimensions 0 and 1 are the
// published sequences. The expected values are written here in closed form and
// do not use any code from the sampler.
#include "sampling_test_util.h"

using namespace sptest;

namespace {

    // Dimension 0: the van der Corput sequence, i.e. the bits of i mirrored about
    // the binary point.
    double vanDerCorput(uint32_t i) {
        double x = 0.0, w = 0.5;
        for (; i; i >>= 1, w *= 0.5)
            if (i & 1u)
                x += w;
        return x;
    }

    // Dimension 1 in natural (index) order: the Sobol generator matrix of the
    // second dimension is the Pascal matrix mod 2, so output bit j (counted from
    // the binary point) is XOR over the set bits k of i of C(k, j) mod 2. By
    // Lucas' theorem C(k, j) is odd exactly when (j & k) == j.
    double sobolDimension1(uint32_t i) {
        double x = 0.0;
        for (uint32_t j = 0; j < 32; ++j) {
            unsigned bit = 0;
            for (uint32_t k = 0; k < 32; ++k)
                if ((i >> k) & 1u)
                    bit ^= ((j & k) == j) ? 1u : 0u;
            if (bit)
                x += std::ldexp(1.0, -static_cast<int>(j) - 1);
        }
        return x;
    }

    void sobolReference() {
        // First eight values of dimension 1, written out by hand:
        // 0, 1/2, 3/4, 1/4, 5/8, 1/8, 3/8, 7/8.
        const double hand[8] = {0.0, 0.5, 0.75, 0.25, 0.625, 0.125, 0.375, 0.875};
        for (uint32_t i = 0; i < 8; ++i)
            SP_CHECK(sobolDimension1(i) == hand[i]);

        for (uint32_t i = 0; i < 64; ++i) {
            const sp::Sample2D s = sp::detail::sobolUnscrambled(i);
            if (!(s.u == static_cast<float>(vanDerCorput(i)))) {
                std::fprintf(stderr, "i=%u: dim0 got %.9g want %.9g\n", i, s.u, vanDerCorput(i));
                ++failureCount();
            }
            if (!(s.v == static_cast<float>(sobolDimension1(i)))) {
                std::fprintf(stderr, "i=%u: dim1 got %.9g want %.9g\n", i, s.v, sobolDimension1(i));
                ++failureCount();
            }
        }
    }

} // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"sobol_reference", sobolReference}});
}
