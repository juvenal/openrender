/*
 * Spec 035, Spectrum_Basics (labels common;closures; tests/closures/CMakeLists.txt registers it): the Infra Spectrum
 * (src/common/spectrum.h, data-model.md "Spectrum"). Header-only: the program uses only the header, plus the support header of
 * tests/closures (libRIBCommon, for its diagnostic sink).
 *
 * Written red first: it does not compile until common/spectrum.h exists.
 */
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "common/spectrum.h"
#include "closures/test_support.h"

using openrender::kSpectrumChannels;
using openrender::Spectrum;

// FR-005: the C boundary is `const float *` plus a count. A Spectrum is exactly its channels, never passed by value to a C table.
static_assert(kSpectrumChannels == 3, "the compile-time channel count is 3 (D18)");
static_assert(sizeof(Spectrum) == kSpectrumChannels * sizeof(float), "a Spectrum is its channels and nothing else");
static_assert(std::is_standard_layout_v<Spectrum> && std::is_trivially_copyable_v<Spectrum>, "a Spectrum can cross a C boundary as its float array");
static_assert(std::is_same_v<decltype(Spectrum::fromArray3(std::declval<const float *>())), Spectrum>, "fromArray3 takes const float *");
static_assert(std::is_same_v<decltype(std::declval<const Spectrum &>().toArray3(std::declval<float *>())), void>, "toArray3 writes a float array");

static Spectrum make(float a, float b, float c) { return Spectrum::fromRGB(a, b, c); }

static void checkSame(const char *what, const Spectrum &s, float a, float b, float c) {
    (void)what;
    CL_CHECK_BITS(s.c[0], a);
    CL_CHECK_BITS(s.c[1], b);
    CL_CHECK_BITS(s.c[2], c);
}

int main() {
    // construction and the two conversions
    const Spectrum zero;
    checkSame("default", zero, 0, 0, 0);
    const float arr[3] = {0.25f, 0.5f, 2.0f};
    const Spectrum fromArr = Spectrum::fromArray3(arr);
    checkSame("fromArray3", fromArr, 0.25f, 0.5f, 2.0f);
    float back[3] = {-1, -1, -1};
    fromArr.toArray3(back);
    CL_CHECK_BITS(back[0], 0.25f);
    CL_CHECK_BITS(back[1], 0.5f);
    CL_CHECK_BITS(back[2], 2.0f);
    const Spectrum rgb = Spectrum::fromRGB(0.125f, 0.75f, 4.0f);
    checkSame("fromRGB", rgb, 0.125f, 0.75f, 4.0f);
    float r = 0, g = 0, b = 0;
    rgb.toRGB(r, g, b);
    CL_CHECK_BITS(r, 0.125f);
    CL_CHECK_BITS(g, 0.75f);
    CL_CHECK_BITS(b, 4.0f);

    // per channel arithmetic (every operand is exact in binary, so the results are compared bit for bit)
    const Spectrum x = make(1, 2, 4), y = make(0.5f, 0.25f, 8);
    checkSame("+", x + y, 1.5f, 2.25f, 12);
    checkSame("-", x - y, 0.5f, 1.75f, -4);
    checkSame("*", x * y, 0.5f, 0.5f, 32);
    checkSame("* float", x * 2.0f, 2, 4, 8);
    checkSame("/ float", x / 2.0f, 0.5f, 1, 2);

    // reductions
    CL_CHECK_BITS(make(0.75f, 0.25f, 0.5f).mean(), 0.5f);
    CL_CHECK_BITS(make(0.75f, 3.0f, 0.5f).maxChannel(), 3.0f);
    CL_CHECK_BITS(make(-4.0f, -3.0f, -2.0f).maxChannel(), -2.0f);

    // predicates
    CL_CHECK(zero.isZero());
    CL_CHECK(!make(0, 0, 1e-30f).isZero());
    CL_CHECK(make(0.0f, -0.0f, 0.0f).isZero()); // negative zero is zero
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    CL_CHECK(x.isFinite());
    CL_CHECK(!make(1, nan, 1).isFinite());
    CL_CHECK(!make(1, 1, inf).isFinite());
    CL_CHECK(!make(-inf, 1, 1).isFinite());
    CL_CHECK(x.isNonNegative());
    CL_CHECK(zero.isNonNegative());
    CL_CHECK(!make(1, -1e-30f, 1).isNonNegative());
    CL_CHECK(!make(nan, 1, 1).isNonNegative()); // NaN is not a non-negative number

    // the result type of an expression is a Spectrum, and the operators do not touch their operands
    const Spectrum keep = x;
    const Spectrum sum = x + y;
    (void)sum;
    checkSame("operands untouched", x, keep.c[0], keep.c[1], keep.c[2]);

    return CL_RESULT("Spectrum_Basics");
}
