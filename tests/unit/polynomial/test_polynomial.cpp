/**
 * Project: openRender
 *
 * File: test_polynomial.cpp
 *
 * Description:
 *   Unit test for the closed-form polynomial solvers in common/polynomial.h
 *   (solveQuadric, solveCubic, solveQuartic). solveQuartic's only caller is
 *   CToroid::intersect(), so every lost root is a torus pixel that never
 *   renders. Regression for a `cbrt` macro that left its argument
 *   unparenthesized: cbrt(sqrt_D + q) expanded to pow(-sqrt_D + q, 1/3), a
 *   NaN whenever the resolvent cubic had one real root and sqrt_D + q < 0,
 *   which made solveQuartic() report no roots for 77% of the rays that hit
 *   a torus.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "common/polynomial.h"

// ---------------------------------------------------------------------------
// Minimal test framework (the style of the other unit suites)
// ---------------------------------------------------------------------------

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                              \
    void test_##name();                         \
    void run_test_##name() {                    \
        printf("Running test: %s ... ", #name); \
        fflush(stdout);                         \
        int failedBefore = tests_failed;        \
        test_##name();                          \
        if (tests_failed == failedBefore) {     \
            tests_passed++;                     \
            printf("PASSED\n");                 \
        }                                       \
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

#define RUN(name) run_test_##name()

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// The real roots solveQuartic() reports for c[0] + c[1] t + ... + c[4] t^4, sorted
static std::vector<double> quarticRoots(double c[5]) {
    double coeffs[5], s[4];
    std::copy(c, c + 5, coeffs);
    const int n = solveQuartic<double>(coeffs, s);
    std::vector<double> out(s, s + n);
    std::sort(out.begin(), out.end());
    return out;
}

// Multiply polynomial p (ascending coefficients) by (t^2 + b t + c)
static std::vector<double> mulQuadratic(const std::vector<double> &p, double b, double c) {
    std::vector<double> r(p.size() + 2, 0.0);
    for (size_t i = 0; i < p.size(); i++) {
        r[i] += p[i] * c;
        r[i + 1] += p[i] * b;
        r[i + 2] += p[i];
    }
    return r;
}

static bool closeTo(double a, double b) {
    return std::fabs(a - b) <= 1e-6 * std::max(1.0, std::fabs(b));
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// polynomial.h must not define a function-like `cbrt` macro: it shadows
// std::cbrt in every file that includes it, and the one it defined expanded
// a negated argument without parentheses
TEST(header_defines_no_cbrt_macro) {
#ifdef cbrt
    const bool cbrtIsAMacro = true;
#else
    const bool cbrtIsAMacro = false;
#endif
    ASSERT(!cbrtIsAMacro);
}

// One real root, reached through the D > 0 branch with sqrt_D + q < 0:
// (t - 2)(t^2 + 1) = t^3 - 2 t^2 + t - 2
TEST(cubic_one_real_root_negative_cbrt_argument) {
    double c[4] = {-2, 1, -2, 1}, s[3];
    const int n = solveCubic<double>(c, s);
    ASSERT(n == 1);
    ASSERT(std::isfinite(s[0]));
    ASSERT(closeTo(s[0], 2.0));
}

// The resolvent cubic of the quartic below, verbatim: it took the NaN path
TEST(cubic_resolvent_of_the_torus_axis_ray) {
    const double p = 3.875, q = 0, r = -0.24609375;
    double c[4] = {0.5 * r * p - 0.125 * q * q, -r, -0.5 * p, 1}, s[3];
    const int n = solveCubic<double>(c, s);
    ASSERT(n >= 1);
    for (int i = 0; i < n; i++) {
        ASSERT(std::isfinite(s[i]));
        const double v = ((c[3] * s[i] + c[2]) * s[i] + c[1]) * s[i] + c[0];
        ASSERT(std::fabs(v) < 1e-9);
    }
}

// The ray (0, 1, -5) + t (0, 0, 1) through a torus (major radius 1, tube
// 0.25, axis z) crosses the tube at z = -0.25 and z = +0.25: t = 4.75, 5.25
TEST(quartic_torus_ray_through_the_tube_centre_circle) {
    double c[5] = {721.62890625, -538.75, 153.875, -20, 1};
    const std::vector<double> roots = quarticRoots(c);
    ASSERT(roots.size() == 2);
    ASSERT(closeTo(roots[0], 4.75));
    ASSERT(closeTo(roots[1], 5.25));
}

// Four, two and no real roots, built from known factors
TEST(quartic_known_roots) {
    {
        // (t - 1)(t - 2)(t - 3)(t - 4)
        double c[5] = {24, -50, 35, -10, 1};
        const std::vector<double> roots = quarticRoots(c);
        ASSERT(roots.size() == 4);
        for (int i = 0; i < 4; i++)
            ASSERT(closeTo(roots[i], i + 1.0));
    }
    {
        // (t + 0.5)(t - 3)(t^2 + 2 t + 5): two real roots and a complex pair
        const std::vector<double> p = mulQuadratic(mulQuadratic({1}, -2.5, -1.5), 2, 5);
        double c[5] = {p[0], p[1], p[2], p[3], p[4]};
        const std::vector<double> roots = quarticRoots(c);
        ASSERT(roots.size() == 2);
        ASSERT(closeTo(roots[0], -0.5));
        ASSERT(closeTo(roots[1], 3.0));
    }
    {
        // (t^2 + 1)(t^2 + t + 3): no real roots
        const std::vector<double> p = mulQuadratic(mulQuadratic({1}, 0, 1), 1, 3);
        double c[5] = {p[0], p[1], p[2], p[3], p[4]};
        ASSERT(quarticRoots(c).empty());
    }
}

// Random quartics with two separated real roots and a complex pair, and with
// four separated real roots: every real root found, to 1e-6
TEST(quartic_randomized_against_known_roots) {
    std::mt19937 rng(21);
    std::uniform_real_distribution<double> U(-4, 4);
    int failures = 0;
    for (int trial = 0; trial < 20000; trial++) {
        std::vector<double> expected;
        std::vector<double> p = {1};
        if (trial % 2 == 0) {
            const double a = U(rng), b = a + 0.05 + std::fabs(U(rng));
            const double re = U(rng), im = 0.05 + std::fabs(U(rng));
            p = mulQuadratic(p, -(a + b), a * b);
            p = mulQuadratic(p, -2 * re, re * re + im * im);
            expected = {a, b};
        }
        else {
            double r[4];
            for (double &x : r)
                x = U(rng);
            std::sort(r, r + 4);
            bool separated = true;
            for (int i = 1; i < 4; i++)
                separated = separated && r[i] - r[i - 1] > 0.05;
            if (!separated)
                continue;
            p = mulQuadratic(mulQuadratic(p, -(r[0] + r[1]), r[0] * r[1]), -(r[2] + r[3]), r[2] * r[3]);
            expected.assign(r, r + 4);
        }
        double c[5] = {p[0], p[1], p[2], p[3], p[4]};
        const std::vector<double> roots = quarticRoots(c);
        bool ok = roots.size() == expected.size();
        for (size_t i = 0; ok && i < roots.size(); i++)
            ok = std::fabs(roots[i] - expected[i]) <= 1e-6 * std::max(1.0, std::fabs(expected[i]));
        failures += ok ? 0 : 1;
    }
    if (failures != 0)
        printf("\n  %d randomized quartics lost or misplaced a root", failures);
    ASSERT(failures == 0);
}

int main() {
    RUN(header_defines_no_cbrt_macro);
    RUN(cubic_one_real_root_negative_cbrt_argument);
    RUN(cubic_resolvent_of_the_torus_axis_ray);
    RUN(quartic_torus_ray_through_the_tube_centre_circle);
    RUN(quartic_known_roots);
    RUN(quartic_randomized_against_known_roots);
    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
