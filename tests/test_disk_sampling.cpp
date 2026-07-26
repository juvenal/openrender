/**
 * Project: openRender
 *
 * File: test_disk_sampling.cpp
 *
 * Description:
 *   Unit tests for sampleDisk() (src/ri/random.h): bounds/finiteness at the
 *   aperture-edge/near-pinhole extremes (FR-007), and area-uniformity of the
 *   generated distribution via a chi-square goodness-of-fit test on r²
 *   (research.md §6 step 1).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "../src/ri/random.h"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                              \
    void test_##name();                         \
    void run_test_##name() {                    \
        printf("Running test: %s ... ", #name); \
        fflush(stdout);                         \
        test_##name();                          \
        tests_passed++;                         \
        printf("PASSED\n");                     \
    }                                           \
    void test_##name()

#define ASSERT(condition)                                          \
    do {                                                           \
        if (!(condition)) {                                        \
            printf("\nAssertion failed: %s\nFile: %s, Line: %d\n", \
                   #condition, __FILE__, __LINE__);                \
            tests_failed++;                                        \
            return;                                                \
        }                                                          \
    } while (0)

////////////////////////////////////////////////////////////////////////
// Deterministic uniform[0,1) sampler, standing in for either hider's
// real source (CSobol<2> / urand()) — sampleDisk() is templated on the
// sampler so any callable writing two independent U(0,1) values works.
////////////////////////////////////////////////////////////////////////

namespace {

struct MT19937Sampler {
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist{0.0f, 1.0f};

    explicit MT19937Sampler(unsigned seed) : rng(seed) {}

    void operator()(float *out) {
        out[0] = dist(rng);
        out[1] = dist(rng);
    }
};

// Regularized upper incomplete gamma function Q(a, x) = 1 - P(a, x),
// via series (for x < a+1) or continued fraction (otherwise), following
// the standard Numerical Recipes formulation. Used to convert a chi-square
// statistic into a p-value without a canned statistics library dependency
// (Constitution Principle V — minimal dependencies).
double gammaSeriesP(double a, double x) {
    const int ITMAX = 200;
    const double EPS = 3.0e-9;

    double gln = std::lgamma(a);
    if (x <= 0.0) return 0.0;

    double ap = a;
    double sum = 1.0 / a;
    double del = sum;
    for (int n = 1; n <= ITMAX; ++n) {
        ap += 1.0;
        del *= x / ap;
        sum += del;
        if (std::fabs(del) < std::fabs(sum) * EPS) break;
    }
    return sum * std::exp(-x + a * std::log(x) - gln);
}

double gammaContinuedFractionQ(double a, double x) {
    const int ITMAX = 200;
    const double EPS = 3.0e-9;
    const double FPMIN = 1.0e-300;

    double gln = std::lgamma(a);
    double b = x + 1.0 - a;
    double c = 1.0 / FPMIN;
    double d = 1.0 / b;
    double h = d;
    for (int i = 1; i <= ITMAX; ++i) {
        double an = -i * (i - a);
        b += 2.0;
        d = an * d + b;
        if (std::fabs(d) < FPMIN) d = FPMIN;
        c = b + an / c;
        if (std::fabs(c) < FPMIN) c = FPMIN;
        d = 1.0 / d;
        double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < EPS) break;
    }
    return std::exp(-x + a * std::log(x) - gln) * h;
}

// Q(a, x): upper tail of the regularized incomplete gamma function.
double gammaQ(double a, double x) {
    if (x < a + 1.0) {
        return 1.0 - gammaSeriesP(a, x);
    }
    return gammaContinuedFractionQ(a, x);
}

// p-value for a chi-square statistic with k degrees of freedom.
double chiSquarePValue(double chiSquareStat, int degreesOfFreedom) {
    return gammaQ(0.5 * degreesOfFreedom, 0.5 * chiSquareStat);
}

} // namespace

////////////////////////////////////////////////////////////////////////
// Test 1: Bounds and finiteness at the aperture-edge/near-pinhole extremes
////////////////////////////////////////////////////////////////////////

TEST(disk_sampling_bounds_and_finiteness) {
    MT19937Sampler sampler(12345u);

    const int N = 20000;
    for (int i = 0; i < N; ++i) {
        float R[2];
        sampleDisk(R, sampler);

        ASSERT(std::isfinite(R[0]));
        ASSERT(std::isfinite(R[1]));

        const float r2 = R[0] * R[0] + R[1] * R[1];
        ASSERT(r2 < 1.0f);
    }
}

////////////////////////////////////////////////////////////////////////
// Test 2: Area-uniformity via chi-square goodness-of-fit on r²
//
// Area-uniform disk sampling makes r² itself uniform on [0,1) — a
// center-biased (linear-r) generator fails this check while a correct one
// passes within a standard statistical tolerance (research.md §6 step 1):
// >=1000 samples, 8-16 equal-width r² buckets, p > 0.01 to accept
// uniformity (fail to reject the null hypothesis).
////////////////////////////////////////////////////////////////////////

TEST(disk_sampling_area_uniformity_chisquare) {
    MT19937Sampler sampler(67890u);

    const int N = 4000;
    const int BINS = 12;
    std::vector<int> counts(BINS, 0);

    for (int i = 0; i < N; ++i) {
        float R[2];
        sampleDisk(R, sampler);

        const float r2 = R[0] * R[0] + R[1] * R[1];
        int bin = static_cast<int>(r2 * BINS);
        if (bin >= BINS) bin = BINS - 1; // r2 can equal 1.0 - epsilon at most
        if (bin < 0) bin = 0;
        counts[bin]++;
    }

    const double expected = static_cast<double>(N) / BINS;
    double chiSquareStat = 0.0;
    for (int b = 0; b < BINS; ++b) {
        const double diff = counts[b] - expected;
        chiSquareStat += (diff * diff) / expected;
    }

    const int degreesOfFreedom = BINS - 1;
    const double pValue = chiSquarePValue(chiSquareStat, degreesOfFreedom);

    printf("\n  chi-square = %.4f, df = %d, p = %.4f", chiSquareStat, degreesOfFreedom, pValue);
    ASSERT(pValue > 0.01);
}

////////////////////////////////////////////////////////////////////////
// Main Test Runner
////////////////////////////////////////////////////////////////////////

int main() {
    printf("========================================\n");
    printf("openRender sampleDisk() Test Suite\n");
    printf("========================================\n\n");

    run_test_disk_sampling_bounds_and_finiteness();
    run_test_disk_sampling_area_uniformity_chisquare();

    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("========================================\n");

    if (tests_failed > 0) {
        printf("FAILURE: Some tests failed!\n");
        return 1;
    }

    printf("SUCCESS: All tests passed!\n");
    return 0;
}
