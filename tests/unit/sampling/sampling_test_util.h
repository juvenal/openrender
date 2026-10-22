// Shared helpers for the spec 039 sampler tests (tests/unit/sampling).
//
// The tests are written against contracts/sampler-api.md before the sampler
// exists; they include "common/sampling.h" and so do not compile until
// src/common/sampling.h is added (Phase 2, T018).
#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "common/sampling.h"

namespace sptest {

namespace sp = openrender::sampling;

inline int &failureCount() {
    static int n = 0;
    return n;
}

#define SP_CHECK(cond)                                                          \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++sptest::failureCount();                                           \
        }                                                                       \
    } while (0)

// Largest value a draw may return: 1 - 2^-24 (spec FR-004).
inline constexpr float kMaxDraw = 1.0f - 0x1p-24f;

inline bool inRange(float v) { return v >= 0.0f && v <= kMaxDraw; }  // false for NaN

inline uint32_t bitsOf(float v) { return std::bit_cast<uint32_t>(v); }

inline uint64_t splitmix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

inline constexpr sp::Backend kBackends[] = {sp::Backend::Sobol, sp::Backend::PMJ02,
                                            sp::Backend::Independent};

inline const char *backendName(sp::Backend b) {
    switch (b) {
    case sp::Backend::Sobol: return "Sobol";
    case sp::Backend::PMJ02: return "PMJ02";
    case sp::Backend::Independent: return "Independent";
    default: return "Other";
    }
}

// A sub-stream that starts at index 0, for tests that exercise the draw
// functions without the identity layer.
inline sp::SubstreamKey rawKey(uint64_t n) { return sp::SubstreamKey{splitmix(n), 0u}; }

// Regularised upper incomplete gamma Q(a, x), after Numerical Recipes 6.2.
inline double gammaQ(double a, double x) {
    if (x <= 0.0) return 1.0;
    const double gln = std::lgamma(a);
    if (x < a + 1.0) {
        double ap = a, sum = 1.0 / a, del = sum;
        for (int n = 0; n < 1000; ++n) {
            ap += 1.0;
            del *= x / ap;
            sum += del;
            if (std::fabs(del) < std::fabs(sum) * 1e-14) break;
        }
        return 1.0 - sum * std::exp(-x + a * std::log(x) - gln);
    }
    double b = x + 1.0 - a, c = 1.0 / 1e-300, d = 1.0 / b, h = d;
    for (int i = 1; i < 1000; ++i) {
        const double an = -i * (i - a);
        b += 2.0;
        d = an * d + b;
        if (std::fabs(d) < 1e-300) d = 1e-300;
        c = b + an / c;
        if (std::fabs(c) < 1e-300) c = 1e-300;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < 1e-14) break;
    }
    return std::exp(-x + a * std::log(x) - gln) * h;
}

// p-value of a chi-square statistic with `dof` degrees of freedom.
inline double chiSquareP(double chi2, int dof) { return gammaQ(0.5 * dof, 0.5 * chi2); }

// A test executable runs one named case, or its only case when no argument
// is given. Returns the process exit code (0 = pass).
using CaseFn = std::function<void()>;

inline int runCase(int argc, char **argv, const std::vector<std::pair<std::string, CaseFn>> &cases) {
    const CaseFn *chosen = nullptr;
    if (argc >= 2) {
        for (const auto &c : cases)
            if (c.first == argv[1]) chosen = &c.second;
        if (!chosen) {
            std::fprintf(stderr, "unknown case '%s'\n", argv[1]);
            return 2;
        }
    } else if (cases.size() == 1) {
        chosen = &cases.front().second;
    } else {
        std::fprintf(stderr, "usage: %s <case>\n", argv[0]);
        return 2;
    }
    (*chosen)();
    if (failureCount() != 0) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failureCount());
        return 1;
    }
    return 0;
}

}  // namespace sptest
