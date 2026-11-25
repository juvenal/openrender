/*
 * Spec 035 (closure tree and BSDF), Phase 2: the shared numerical checks of the Bsdf_* tests (contracts/closure-abi.md "Numerical
 * acceptance", SC-004), written ONCE here and used by every component's test.
 *
 * What is compared, and why no tolerance is a free constant. A tolerance is one of two things, both written where it is used:
 *   - a FLOAT tolerance, `floatTol(a, b) = 512 * FLT_EPSILON * (|a| + |b|) + 1e-7`: two evaluations of the same quantity by different
 *     operand orders differ by float rounding, a few ulps per operation, and 512 epsilon (6.1e-5 relative) bounds a formula of a few dozen
 *     operations (D, G, F, a division by two cosines); the 1e-7 absolute floor is for a value that is exactly zero on one side;
 *   - a STATISTICAL tolerance, `3 * sigma`, where sigma is the standard error of the estimator that produced the number, computed from
 *     its own sample variance and sample count (sigma = sd / sqrt(N)); two estimators are compared with `3 * sqrt(sigma1^2 + sigma2^2)`.
 *   - the chi-square test at significance 0.001 has the critical value of the chi-square distribution with (bins - 1) degrees of freedom,
 *     from the Wilson-Hilferty approximation with z = 3.0902 (the 0.999 quantile of the normal distribution).
 * A check that fails is reported with both values and sigma; nothing in this header is ever widened (a Linux-only failure is reported
 * with the observed value and the sigma).
 *
 * The random numbers come from common/sampling.h's keyed stream with TEST-LOCAL keys (cltest::KeyedRandom, whose seeds are constants of
 * the tests): this header defines no call site in sampling.h (FR-008). Energy and the white-furnace checks run in IMPORTANCE transport
 * mode (radiance mode multiplies a transmissive lobe by (eta_wo/eta_wi)^2, so its albedo may exceed 1 by design: A1, maintainer
 * 2026-11-23); reciprocity runs in both modes.
 *
 * The notation of the contract is kept: wo and wi point away from the surface, f excludes |cos theta_i|, densities are per steradian,
 * and a component works in its own local frame, whose z axis is the component's normal.
 */
#ifndef CLOSURES_BSDF_CHECKS_H
#define CLOSURES_BSDF_CHECKS_H

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "common/memory.h"
#include "bsdf.hpp"
#include "bsdfMath.h"
#include "closureAbi.h"
#include "closureBuiltins.h"
#include "closureRegistry.h"
#include "closureTree.h"
#include "closures/test_support.h"

namespace clbsdf {

constexpr double kPi = 3.14159265358979323846;

struct V3 {
        float x, y, z;
};

inline V3 normalized(V3 v) {
    const double l = std::sqrt((double)v.x * v.x + (double)v.y * v.y + (double)v.z * v.z);
    return V3{(float)(v.x / l), (float)(v.y / l), (float)(v.z / l)};
}
inline V3 fromCosAzimuth(double cosTheta, double phi) {
    const double s = std::sqrt(std::fmax(0.0, 1.0 - cosTheta * cosTheta));
    return V3{(float)(s * std::cos(phi)), (float)(s * std::sin(phi)), (float)cosTheta};
}
inline V3 uniformSphere(float u1, float u2) { return fromCosAzimuth(1.0 - 2.0 * (double)u1, 2.0 * kPi * (double)u2); }
inline V3 uniformUpperHemisphere(float u1, float u2) { return fromCosAzimuth((double)u1, 2.0 * kPi * (double)u2); }
inline const float *p(const V3 &v) { return &v.x; }

inline OrTransportMode dual(OrTransportMode m) { return m == OR_TRANSPORT_RADIANCE ? OR_TRANSPORT_IMPORTANCE : OR_TRANSPORT_RADIANCE; }
inline const char *modeName(OrTransportMode m) { return m == OR_TRANSPORT_RADIANCE ? "RADIANCE" : "IMPORTANCE"; }

// the rounding floor added to a 3 sigma bound whose sigma can be (almost) zero: an estimator whose every term is the same quotient
// (a diffuse lobe's f |cos| / pdf is 1 for every sample) has a sigma of float rounding only, and the mean of rounded terms is off by at
// most a few ulps of 1: 4 epsilon
constexpr double kRoundingFloor = 4.0 * FLT_EPSILON;

// the float rounding tolerance (see the header comment)
inline double floatTol(double a, double b) { return 512.0 * FLT_EPSILON * (std::fabs(a) + std::fabs(b)) + 1e-7; }

// What a check is run against: a component through its operations (local frame) or a prepared tree through the table
struct Subject {
        std::string name;
        std::function<OrBsdfEval(const V3 &wo, const V3 &wi, OrTransportMode)> evaluate;
        std::function<OrBsdfEval(const V3 &wo, const V3 &wi, OrTransportMode)> pdf;
        std::function<OrBsdfSample(const V3 &wo, float u1, float u2, float u3, OrTransportMode)> sample;
        bool reflectionOnly = true; // f is symmetric in wo and wi, in both modes, and wo and wi live in the upper hemisphere
        bool lossless = false;      // the directional albedo is exactly 1 (a furnace test applies)
};

// A component's operations on a block, in its local frame (u3 is not used by a component)
inline Subject componentSubject(const char *name, int32_t closureId, const void *block) {
    const OrClosureEntry *e = openrender::shading::closureEntry(closureId);
    Subject s;
    s.name = name;
    s.evaluate = [e, block](const V3 &wo, const V3 &wi, OrTransportMode m) {
        OrBsdfEval out;
        std::memset(&out, 0, sizeof(out));
        e->ops->evaluate(block, p(wo), p(wi), m, &out);
        return out;
    };
    s.pdf = [e, block](const V3 &wo, const V3 &wi, OrTransportMode m) {
        OrBsdfEval out;
        std::memset(&out, 0, sizeof(out));
        e->ops->pdf(block, p(wo), p(wi), m, &out);
        return out;
    };
    s.sample = [e, block](const V3 &wo, float u1, float u2, float, OrTransportMode m) {
        OrBsdfSample out;
        std::memset(&out, 0, sizeof(out));
        e->ops->sample(block, p(wo), u1, u2, m, &out);
        return out;
    };
    return s;
}

// ---- a prepared tree through the table (the shading frame is the local frame: N = z, dPdu = x, I = -z)
inline void *arenaAllocate(void *context, size_t size, size_t align) {
    CMemPage *&arena = *(CMemPage **)context;
    char *raw = (char *)ralloc((int)(size + align), arena);
    return (void *)(((uintptr_t)raw + (align - 1)) & ~(uintptr_t)(align - 1));
}

inline OrStatus prepareTree(const OrClosureNode *tree, CMemPage *&arena, OrBsdf **out) {
    OrShadingFrame frame = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}};
    OrClosureAllocator alloc = {&arena, arenaAllocate};
    return orBsdfTable()->prepare(tree, &frame, &alloc, out);
}

inline OrBsdfQuery makeQuery(const V3 &wo, const V3 &wi, OrTransportMode m, uint32_t requested = OR_BSDF_ALL) {
    OrBsdfQuery q;
    std::memset(&q, 0, sizeof(q));
    q.structSize = sizeof(q);
    q.wo[0] = wo.x; q.wo[1] = wo.y; q.wo[2] = wo.z;
    q.wi[0] = wi.x; q.wi[1] = wi.y; q.wi[2] = wi.z;
    q.Ng[2] = 1.0f;
    q.mode = m;
    q.requested = requested;
    return q;
}

// A prepared tree through the table's evaluate, pdf and sample (the tree's sample takes u3 as the lobe number uLobe)
inline Subject tableSubject(const char *name, const OrBsdf *bsdf, uint32_t requested = OR_BSDF_ALL) {
    Subject s;
    s.name = name;
    s.evaluate = [bsdf, requested](const V3 &wo, const V3 &wi, OrTransportMode m) {
        OrBsdfEval out;
        std::memset(&out, 0, sizeof(out));
        const OrBsdfQuery q = makeQuery(wo, wi, m, requested);
        orBsdfTable()->evaluate(bsdf, &q, &out);
        return out;
    };
    s.pdf = [bsdf, requested](const V3 &wo, const V3 &wi, OrTransportMode m) {
        OrBsdfEval out;
        std::memset(&out, 0, sizeof(out));
        const OrBsdfQuery q = makeQuery(wo, wi, m, requested);
        orBsdfTable()->pdf(bsdf, &q, &out);
        return out;
    };
    s.sample = [bsdf, requested](const V3 &wo, float u1, float u2, float u3, OrTransportMode m) {
        OrBsdfSample out;
        std::memset(&out, 0, sizeof(out));
        const OrBsdfQuery q = makeQuery(wo, V3{0, 0, 0}, m, requested);
        orBsdfTable()->sample(bsdf, &q, u1, u2, u3, &out);
        return out;
    };
    return s;
}

struct Options {
        std::vector<double> woCos = {0.95, 0.5, 0.12}; // the cosines of the wo directions tried (a negative one is inside, transmissive lobes only)
        int samples = 100000;                          // the sampling estimators
        int uniformSamples = 200000;                   // the uniform-sphere estimators
        int pairs = 20000;                             // the pointwise checks
        bool crossEstimators = true;                   // the uniform-sphere estimators (off for a lobe too peaked for one to have a usable sigma)
        bool albedoBound = true;                       // the sampled albedo is at most 1 + 3 sigma (off where the sampler's weight is unbounded and sigma unreliable: D3)
        bool chiSquare = true;                         // the chi-square test (off for a lobe whose density a 8 x 8 grid cannot integrate)
        int chiSamples = 400000;
        double chiLevel = 0.001; // the significance of one chi-square check (a test that makes many raises the family-wise level 0.001 to a per-row one: see test_bsdf_microfacet.cpp)
        std::function<std::vector<double>(double woZ)> zEdges; // chi-square: the z of every jump of the density for a wo.z (none: no jump); see the helper of the test that sets it
};

struct Estimate {
        double value, sigma;
};

// mean and standard error of a series given by sums
inline Estimate fromSums(double sum, double sumSq, double n) {
    const double mean = sum / n;
    const double var = std::fmax(0.0, sumSq / n - mean * mean) * n / (n - 1.0);
    return Estimate{mean, std::sqrt(var / n)};
}

inline void printEstimate(const char *what, const std::string &name, double woZ, const Estimate &e) {
    std::printf("  %-14s %-34s wo.z %+.2f  value %.6f  sigma %.2e\n", what, name.c_str(), woZ, e.value, e.sigma);
}

inline V3 woFor(double cosTheta) { return fromCosAzimuth(cosTheta, 0.7); }

// ---- reciprocity: f(wo, wi, mode) == f(wi, wo, dual(mode)) (the adjoint relation: the importance-mode BSDF is the radiance-mode one with
// the directions swapped, Veach 1997 section 5.2); and, for a reflection-only lobe, f(wo, wi, mode) == f(wi, wo, mode) in both modes.
// Half of the pairs are two uniform directions (they cover the zero-zero cases); in the other half `b` is drawn by sample(a, ...), so the
// pair lies in the lobe's support even for a narrow or a transmissive lobe (D1, maintainer 2026-11-25), and that half is guarded against a
// vacuous run: more than pairs / 16 of its pairs must be non-zero.
inline void checkReciprocity(const Subject &s, const Options &o, uint64_t seed) {
    cltest::KeyedRandom rnd(seed, 11);
    int nonZeroSampled = 0;
    for (int i = 0; i < o.pairs; ++i) {
        const V3 a = s.reflectionOnly ? uniformUpperHemisphere(rnd.next(), rnd.next()) : uniformSphere(rnd.next(), rnd.next());
        V3 b = s.reflectionOnly ? uniformUpperHemisphere(rnd.next(), rnd.next()) : uniformSphere(rnd.next(), rnd.next());
        const bool sampledHalf = (i & 1) != 0;
        bool haveSampled = false;
        if (sampledHalf) {
            const OrBsdfSample r = s.sample(a, rnd.next(), rnd.next(), rnd.next(), OR_TRANSPORT_IMPORTANCE);
            if (!(r.flags & (OR_BSDF_FAILED | OR_BSDF_DELTA))) {
                b = V3{r.wi[0], r.wi[1], r.wi[2]};
                haveSampled = true;
            }
        }
        for (OrTransportMode m : {OR_TRANSPORT_RADIANCE, OR_TRANSPORT_IMPORTANCE}) {
            const double fab = s.evaluate(a, b, m).f[0];
            const double fba = s.evaluate(b, a, dual(m)).f[0];
            if (std::fabs(fab - fba) > floatTol(fab, fba)) {
                char t[300];
                std::snprintf(t, sizeof(t), "%s reciprocity f(a,b,%s)=%.9g != f(b,a,%s)=%.9g (tol %.3g)", s.name.c_str(), modeName(m), fab, modeName(dual(m)), fba, floatTol(fab, fba));
                cltest::fail(__FILE__, __LINE__, t);
                return;
            }
            if (s.reflectionOnly) {
                const double fba2 = s.evaluate(b, a, m).f[0];
                if (std::fabs(fab - fba2) > floatTol(fab, fba2)) {
                    char t[300];
                    std::snprintf(t, sizeof(t), "%s symmetry f(a,b,%s)=%.9g != f(b,a,%s)=%.9g", s.name.c_str(), modeName(m), fab, modeName(m), fba2);
                    cltest::fail(__FILE__, __LINE__, t);
                    return;
                }
            }
            if (haveSampled && m == OR_TRANSPORT_IMPORTANCE && fab != 0.0)
                ++nonZeroSampled;
        }
    }
    CL_CHECK(nonZeroSampled > o.pairs / 16); // the sampled half is not vacuous
}

// ---- reverse density: pdfReverse of a query is the forward density of the swapped query, from evaluate and from pdf (section 7.4)
inline void checkReverseDensity(const Subject &s, const Options &o, uint64_t seed) {
    cltest::KeyedRandom rnd(seed, 12);
    for (int i = 0; i < o.pairs; ++i) {
        const V3 a = s.reflectionOnly ? uniformUpperHemisphere(rnd.next(), rnd.next()) : uniformSphere(rnd.next(), rnd.next());
        const V3 b = s.reflectionOnly ? uniformUpperHemisphere(rnd.next(), rnd.next()) : uniformSphere(rnd.next(), rnd.next());
        const OrTransportMode m = (i & 1) ? OR_TRANSPORT_RADIANCE : OR_TRANSPORT_IMPORTANCE;
        const OrBsdfEval eab = s.evaluate(a, b, m), pab = s.pdf(a, b, m), pba = s.pdf(b, a, m);
        const double r1 = eab.pdfReverse, f1 = pba.pdfForward, r2 = pab.pdfReverse, fwd = pab.pdfForward, fwdE = eab.pdfForward;
        if (std::fabs(r1 - f1) > floatTol(r1, f1) || std::fabs(r2 - f1) > floatTol(r2, f1) || std::fabs(fwd - fwdE) > floatTol(fwd, fwdE)) {
            char t[300];
            std::snprintf(t, sizeof(t), "%s reverse density: evaluate.rev %.9g, pdf.rev %.9g, swapped forward %.9g, pdf.fwd %.9g, evaluate.fwd %.9g", s.name.c_str(), r1, r2, f1, fwd,
                          fwdE);
            cltest::fail(__FILE__, __LINE__, t);
            return;
        }
        CL_CHECK(pab.f[0] == 0.0f); // pdf leaves f at 0 (convention)
    }
}

// ---- the sampled estimator of the directional albedo: mean of f |cos theta_i| / pdf over the samples (a failed sample counts 0)
inline Estimate sampledAlbedo(const Subject &s, const V3 &wo, int n, uint64_t seed, OrTransportMode m, double *failFraction = nullptr, double *deltaFraction = nullptr) {
    cltest::KeyedRandom rnd(seed, 21);
    double sum = 0, sumSq = 0;
    int failed = 0, delta = 0;
    for (int i = 0; i < n; ++i) {
        const OrBsdfSample r = s.sample(wo, rnd.next(), rnd.next(), rnd.next(), m);
        double x = 0;
        if (r.flags & OR_BSDF_FAILED)
            ++failed;
        else if (r.flags & OR_BSDF_DELTA) {
            ++delta;
            x = (double)r.f[0] * std::fabs((double)r.wi[2]) / (double)r.pdfForward; // the delta value over its choice probability
        }
        else if (r.pdfForward > 0)
            x = (double)r.f[0] * std::fabs((double)r.wi[2]) / (double)r.pdfForward;
        sum += x;
        sumSq += x * x;
    }
    if (failFraction)
        *failFraction = (double)failed / n;
    if (deltaFraction)
        *deltaFraction = (double)delta / n;
    return fromSums(sum, sumSq, n);
}

// ---- the uniform-sphere estimators: the integral over the sphere of g(wi) is 4 pi times the mean of g
inline Estimate uniformIntegral(int n, uint64_t seed, const std::function<double(const V3 &)> &g) {
    cltest::KeyedRandom rnd(seed, 22);
    double sum = 0, sumSq = 0;
    for (int i = 0; i < n; ++i) {
        const double x = g(uniformSphere(rnd.next(), rnd.next())) * 4.0 * kPi;
        sum += x;
        sumSq += x * x;
    }
    return fromSums(sum, sumSq, n);
}

// ---- energy (IMPORTANCE mode, A1): the sampled albedo is at most 1 + 3 sigma; for a lossless lobe it is 1 within 3 sigma (the white
// furnace); and it agrees with the independent uniform-sphere integral of f |cos| within 3 sqrt(sigma1^2 + sigma2^2)
inline void checkEnergy(const Subject &s, const Options &o, uint64_t seed) {
    for (double c : o.woCos) {
        if (s.reflectionOnly && c < 0)
            continue;
        const V3 wo = woFor(c);
        const Estimate a = sampledAlbedo(s, wo, o.samples, seed ^ (uint64_t)(c * 1000 + 5000), OR_TRANSPORT_IMPORTANCE);
        printEstimate("albedo", s.name, c, a);
        if (o.albedoBound)
            CL_CHECK(a.value <= 1.0 + 3.0 * a.sigma + kRoundingFloor);
        if (s.lossless)
            CL_CHECK_NEAR(a.value, 1.0, 3.0 * a.sigma + kRoundingFloor); // the furnace
        if (o.crossEstimators) {
            const Estimate u = uniformIntegral(o.uniformSamples, seed ^ (uint64_t)(c * 1000 + 9000),
                                               [&](const V3 &wi) { return (double)s.evaluate(wo, wi, OR_TRANSPORT_IMPORTANCE).f[0] * std::fabs((double)wi.z); });
            printEstimate("albedo(uniform)", s.name, c, u);
            CL_CHECK_NEAR(a.value, u.value, 3.0 * std::sqrt(a.sigma * a.sigma + u.sigma * u.sigma) + kRoundingFloor);
        }
    }
}

// ---- density normalisation: the integral of the density over the sphere, plus the probability of a delta choice, plus the probability of
// a failed sample, is 1 (the uniform-sphere integral and the sampled fractions are independent estimators)
inline void checkDensityNorm(const Subject &s, const Options &o, uint64_t seed) {
    if (!o.crossEstimators)
        return;
    for (double c : o.woCos) {
        if (s.reflectionOnly && c < 0)
            continue;
        const V3 wo = woFor(c);
        const Estimate u = uniformIntegral(o.uniformSamples, seed ^ (uint64_t)(c * 1000 + 13000), [&](const V3 &wi) { return (double)s.pdf(wo, wi, OR_TRANSPORT_IMPORTANCE).pdfForward; });
        double failFraction = 0, deltaFraction = 0;
        (void)sampledAlbedo(s, wo, o.samples, seed ^ (uint64_t)(c * 1000 + 17000), OR_TRANSPORT_IMPORTANCE, &failFraction, &deltaFraction);
        const double sigmaFail = std::sqrt(std::fmax(failFraction * (1 - failFraction), 1.0 / o.samples) / o.samples);
        const double sigmaDelta = std::sqrt(std::fmax(deltaFraction * (1 - deltaFraction), 1.0 / o.samples) / o.samples);
        const double total = u.value + failFraction + deltaFraction;
        const double sigma = std::sqrt(u.sigma * u.sigma + sigmaFail * sigmaFail + sigmaDelta * sigmaDelta);
        std::printf("  %-14s %-34s wo.z %+.2f  integral %.6f + failed %.6f + delta %.6f = %.6f  sigma %.2e\n", "pdfnorm", s.name.c_str(), c, u.value, failFraction, deltaFraction, total, sigma);
        CL_CHECK_NEAR(total, 1.0, 3.0 * sigma + kRoundingFloor);
    }
}

// ---- sample / density consistency: the sampled f, pdfForward and pdfReverse equal the independent evaluate and pdf of the sampled direction
inline void checkSampleConsistency(const Subject &s, const Options &o, uint64_t seed) {
    for (double c : o.woCos) {
        if (s.reflectionOnly && c < 0)
            continue;
        const V3 wo = woFor(c);
        cltest::KeyedRandom rnd(seed ^ (uint64_t)(c * 1000 + 21000), 31);
        int used = 0;
        for (int i = 0; i < o.pairs; ++i) {
            const OrTransportMode m = (i & 1) ? OR_TRANSPORT_RADIANCE : OR_TRANSPORT_IMPORTANCE;
            const OrBsdfSample r = s.sample(wo, rnd.next(), rnd.next(), rnd.next(), m);
            if ((r.flags & OR_BSDF_FAILED) || (r.flags & OR_BSDF_DELTA))
                continue;
            ++used;
            const V3 wi{r.wi[0], r.wi[1], r.wi[2]};
            const double len = std::sqrt((double)wi.x * wi.x + (double)wi.y * wi.y + (double)wi.z * wi.z);
            CL_CHECK_NEAR(len, 1.0, 1e-5);
            const OrBsdfEval e = s.evaluate(wo, wi, m), q = s.pdf(wo, wi, m);
            const bool ok = std::fabs(e.f[0] - r.f[0]) <= floatTol(e.f[0], r.f[0]) && std::fabs(q.pdfForward - r.pdfForward) <= floatTol(q.pdfForward, r.pdfForward) &&
                            std::fabs(q.pdfReverse - r.pdfReverse) <= floatTol(q.pdfReverse, r.pdfReverse);
            if (!ok) {
                char t[300];
                std::snprintf(t, sizeof(t), "%s sample vs evaluate/pdf (%s): f %.9g/%.9g fwd %.9g/%.9g rev %.9g/%.9g", s.name.c_str(), modeName(m), r.f[0], e.f[0], r.pdfForward,
                              q.pdfForward, r.pdfReverse, q.pdfReverse);
                cltest::fail(__FILE__, __LINE__, t);
                return;
            }
            CL_CHECK(r.f[0] >= 0 && std::isfinite(r.f[0]) && r.pdfForward > 0 && std::isfinite(r.pdfForward));
        }
        // a lobe that cannot lose samples keeps the pairs / 4 floor; where a sample can fail (a transmissive lobe with wo inside the medium: total
        // internal reflection has no refracted direction) a fixed 1000 usable samples is enough for the pointwise comparison (D2, 2026-11-25)
        const bool canLoseSamples = !s.reflectionOnly && c < 0;
        CL_CHECK(canLoseSamples ? used >= 1000 : used > o.pairs / 4);
    }
}

// the upper-tail point z of the standard normal distribution at significance `level` (the bisection of 0.5 erfc(z / sqrt 2) = level)
inline double normalUpperPoint(double level) {
    double lo = 0.0, hi = 12.0;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (0.5 * std::erfc(mid / std::sqrt(2.0)) > level)
            lo = mid;
        else
            hi = mid;
    }
    return 0.5 * (lo + hi);
}

// the critical value of the chi-square distribution with df degrees of freedom at significance `level` (Wilson-Hilferty; 0.001 is z = 3.0902)
inline double chiSquareCritical(int df, double level = 0.001) {
    const double d = df, z = normalUpperPoint(level), t = 2.0 / (9.0 * d);
    const double r = 1.0 - t + z * std::sqrt(t);
    return d * r * r * r;
}


// The density of each cell of the nz x np grid (z uniform over [-1, 1], phi uniform over [0, 2 pi): equal solid angle), as a midpoint rule of
// sub x sub sub-cells per cell, in two refinements of the plain rule (D6, maintainer 2026-11-25):
//  - the cells that touch a pole (the first and the last in z) are integrated uniformly in theta (weight sin theta) instead of z: the lobe is a
//    smooth function of the direction, which near a pole is a function of sqrt(1 -+ z), and a midpoint rule in z over that is only O(h^1.5),
//    which leaves the sectors of the cap whose density is small short by tens of counts;
//  - every sub-cell that contains one of `edges` (the z of a jump of the density, a ring) is integrated as two pieces, each smooth, so the
//    rule's error is no longer the offset of the jump from a sub-cell midpoint.
// Returns the integral over the sphere; `cells`, if given, receives the nz * np cell integrals.
inline double integrateCells(const Subject &s, const V3 &wo, const std::vector<double> &edges, int nz, int np, int sub, std::vector<double> *cells) {
    if (cells)
        cells->assign(nz * np, 0.0);
    const double dphi = 2.0 * kPi / (np * sub);
    double total = 0;
    for (int iz = 0; iz < nz; ++iz) {
        const bool pole = iz == 0 || iz == nz - 1;
        const double zLo = -1.0 + 2.0 * iz / nz, zHi = -1.0 + 2.0 * (iz + 1) / nz;
        // the integration variable of the cell: theta for a pole cell (acos is decreasing: theta of zHi first), z otherwise
        const double vLo = pole ? std::acos(zHi) : zLo, vHi = pole ? std::acos(zLo) : zHi;
        std::vector<double> cuts; // the edges inside the cell, in the cell's variable
        for (double e : edges)
            if (e > zLo && e < zHi)
                cuts.push_back(pole ? std::acos(e) : e);
        std::sort(cuts.begin(), cuts.end());
        const double dv = (vHi - vLo) / sub;
        for (int i = 0; i < sub; ++i) {
            const double a = vLo + i * dv, b = (i == sub - 1) ? vHi : vLo + (i + 1) * dv;
            std::vector<double> pts{a};
            for (double e : cuts)
                if (e > a && e < b)
                    pts.push_back(e);
            pts.push_back(b);
            for (size_t k = 0; k + 1 < pts.size(); ++k) {
                const double p = pts[k], q = pts[k + 1], v = 0.5 * (p + q), w = q - p;
                const double z = pole ? std::cos(v) : v;
                const double weight = pole ? w * std::sin(v) : w;
                for (int ip = 0; ip < np * sub; ++ip) {
                    const double value = s.pdf(wo, fromCosAzimuth(z, (ip + 0.5) * dphi), OR_TRANSPORT_IMPORTANCE).pdfForward * weight * dphi;
                    total += value;
                    if (cells)
                        (*cells)[iz * np + ip / sub] += value;
                }
            }
        }
    }
    return total;
}

// ---- chi-square: a fixed 8 x 8 histogram of sampled directions (z uniform over [-1, 1], phi uniform over [0, 2 pi): equal solid angle)
// against the density integrated over each cell (integrateCells: a midpoint rule of 48 x 48 points per cell), plus one cell for the failed samples,
// whose expectation is what the density leaves of 1; a cell with an expectation under 5 counts are left out. The failed cell is also left out
// (and logged) when its expectation is smaller than four times the quadrature error of the density integral, estimated as the difference of
// the integral at 48 and at 96 sub-cells per cell: the cell is then a quadrature residue, not a measurement (D4, maintainer 2026-11-25)
inline void checkChiSquare(const Subject &s, const Options &o, uint64_t seed) {
    if (!o.chiSquare)
        return;
    const int nz = 8, np = 8, sub = 48;
    for (double c : o.woCos) {
        if (s.reflectionOnly && c < 0)
            continue;
        const V3 wo = woFor(c);
        std::vector<double> expected, observed(nz * np, 0.0);
        const std::vector<double> edges = o.zEdges ? o.zEdges(wo.z) : std::vector<double>();
        const double total = integrateCells(s, wo, edges, nz, np, sub, &expected);
        cltest::KeyedRandom rnd(seed ^ (uint64_t)(c * 1000 + 25000), 41);
        int failed = 0;
        for (int i = 0; i < o.chiSamples; ++i) {
            const OrBsdfSample r = s.sample(wo, rnd.next(), rnd.next(), rnd.next(), OR_TRANSPORT_IMPORTANCE);
            if ((r.flags & OR_BSDF_FAILED) || (r.flags & OR_BSDF_DELTA)) {
                ++failed;
                continue;
            }
            int iz = (int)((r.wi[2] + 1.0) * 0.5 * nz);
            double phi = std::atan2((double)r.wi[1], (double)r.wi[0]);
            if (phi < 0)
                phi += 2.0 * kPi;
            int ip = (int)(phi / (2.0 * kPi) * np);
            iz = iz < 0 ? 0 : (iz >= nz ? nz - 1 : iz);
            ip = ip < 0 ? 0 : (ip >= np ? np - 1 : ip);
            observed[iz * np + ip] += 1;
        }
        double chi = 0;
        int cells = 0;
        for (int b = 0; b < nz * np; ++b) {
            const double e = expected[b] * o.chiSamples;
            if (e < 5.0)
                continue;
            chi += (observed[b] - e) * (observed[b] - e) / e;
            ++cells;
        }
        const double total96 = integrateCells(s, wo, edges, nz, np, 2 * sub, nullptr);
        const double quadratureError = std::fabs(total96 - total) * o.chiSamples;
        const double eFail = std::fmax(0.0, 1.0 - total) * o.chiSamples;
        if (eFail >= 5.0 && eFail < 4.0 * quadratureError)
            std::printf("  %-14s %-34s wo.z %+.2f  failed cell dropped: expected %.1f < 4 x quadrature error %.1f (integral %.5f at 48, %.5f at 96 sub-cells)\n", "chisquare", s.name.c_str(), c,
                        eFail, 4.0 * quadratureError, total, total96);
        else if (eFail >= 5.0) {
            chi += (failed - eFail) * (failed - eFail) / eFail;
            ++cells;
        }
        const double critical = chiSquareCritical(cells - 1, o.chiLevel);
        std::printf("  %-14s %-34s wo.z %+.2f  chi2 %.1f over %d cells (df %d), critical %.1f at level %.3g, density integral %.5f\n", "chisquare", s.name.c_str(), c, chi, cells, cells - 1, critical, o.chiLevel, total);
        CL_CHECK(chi <= critical);
    }
}

// The six checks of the contract for a subject
inline void runSixChecks(const Subject &s, const Options &o, uint64_t seed) {
    checkReciprocity(s, o, seed);
    checkEnergy(s, o, seed);
    checkDensityNorm(s, o, seed);
    checkSampleConsistency(s, o, seed);
    checkChiSquare(s, o, seed);
    checkReverseDensity(s, o, seed);
}

// One section of a test program = one ctest test = one check over a subject; returns false for a name that is not a check
inline bool runCheckSection(const std::string &which, const Subject &s, const Options &o, uint64_t seed) {
    if (which == "Reciprocity")
        checkReciprocity(s, o, seed);
    else if (which == "Energy")
        checkEnergy(s, o, seed);
    else if (which == "PdfNorm")
        checkDensityNorm(s, o, seed);
    else if (which == "SamplePdf")
        checkSampleConsistency(s, o, seed);
    else if (which == "ChiSquare")
        checkChiSquare(s, o, seed);
    else if (which == "ReversePdf")
        checkReverseDensity(s, o, seed);
    else
        return false;
    return true;
}

} // namespace clbsdf

#endif
