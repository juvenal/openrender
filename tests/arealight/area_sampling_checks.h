/**
 * Project: openRender
 *
 * File: tests/arealight/area_sampling_checks.h
 *
 * Description:
 *   Spec 057 (area-light geometry), Phase 3, tasks.md T041: the checks the AreaSampling_* tests share, over the Geometry
 *   emitter API (emitterRegistry.h, areaSampling.h) and nothing of Shading, Hiders or the core. A test builds primitives
 *   directly (the contract fixtures of tests/geometry_contract), records them as the roots of one light in a
 *   CEmitterRegistry and asks the light's CEmitterSet to resolve and sample.
 *
 *   PRE-DECLARED BOUNDS (fixed before any run, never adjusted; a failure is a defect or a reported finding, not a reason
 *   to move one):
 *     kAreaRelTol            1e-5    areas of exact samplers (float vertices, double arithmetic)
 *     kCentroidTol           1e-3    centroid error of 10^5 Sobol draws, as a fraction of the emitter's size
 *     kDensityRelTol         1e-6    every draw's pdfA against 1 / totalArea
 *     kRoundTripTol          1e-5    P(u,v) of the leaf surface against the drawn point (exact quad and triangle samplers)
 *     kChiSquareCritical     the chi-square quantile for p = 0.001 by degrees of freedom, the table below (a statistic above
 *                            it fails: p < 0.001 under uniformity, over 10^5 draws of a fixed-seed generator)
 *     the subdivision area bias (kSubdivisionAreaTol, 2%) is declared in test_area_sampling_subdiv.cpp
 *   The Sobol sequence of the centroid and density checks and the generator of the frequency checks are the test's own
 *   (not sampling.h's): a change in the renderer's sampler cannot move a result.
 *
 *   The IRayContext double below is a minimal one (a scratch arena and the varying arrays): the contract tests keep theirs
 *   inside each program (tests/geometry_contract/test_intersect_through_ray_context.cpp), and fixtures.h has none.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#ifndef AREALIGHT_AREA_SAMPLING_CHECKS_H
#define AREALIGHT_AREA_SAMPLING_CHECKS_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <thread>
#include <vector>

#include "areaSampling.h"
#include "contract_test.h"
#include "emitterRegistry.h"
#include "fixtures.h"
#include "memory.h"
#include "polygons.h"
#include "rayContext.h"
#include "stateHandles.h"

namespace alsampling {

    // ---- pre-declared bounds -------------------------------------------------------------------------------------------
    static const double kAreaRelTol = 1e-5;
    static const double kCentroidTol = 1e-3;
    static const double kDensityRelTol = 1e-6;
    static const double kRoundTripTol = 1e-5;
    static const int kNumDraws = 100000; // 10^5, Sobol (centroid) and fixed-seed (frequencies)

    // chi-square quantile at p = 0.001 (upper tail), by degrees of freedom 1..20, then 25, 30, 40, 50
    static double chiSquareCritical(int dof) {
        static const double table[] = {0, 10.828, 13.816, 16.266, 18.467, 20.515, 22.458, 24.322, 26.124, 27.877, 29.588,
                                       31.264, 32.909, 34.528, 36.123, 37.697, 39.252, 40.790, 42.312, 43.820, 45.315};
        if (dof >= 1 && dof <= 20)
            return table[dof];
        if (dof == 25)
            return 52.620;
        if (dof == 30)
            return 59.703;
        if (dof == 40)
            return 73.402;
        if (dof == 50)
            return 86.661;
        return -1.0; // not tabulated: the caller picks a tabulated count
    }

    // ---- the test's own sequences -----------------------------------------------------------------------------------------
    // Sobol (0,2) in two dimensions by Gray code: dimension 1 the van der Corput sequence, dimension 2 the second Sobol
    // dimension. Points i = 0 .. n-1.
    class Sobol2D {
        public:
            Sobol2D() : x1(0), x2(0), index(0) {
                for (int k = 0; k < 32; k++) {
                    v1[k] = 1u << (31 - k);
                    v2[k] = (k == 0) ? (1u << 31) : (v2[k - 1] ^ (v2[k - 1] >> 1));
                }
            }
            void next(double &a, double &b) {
                a = x1 * (1.0 / 4294967296.0);
                b = x2 * (1.0 / 4294967296.0);
                // advance to point index + 1 by Gray code: flip the direction number of the lowest zero bit of index
                uint32_t c = 0;
                for (uint32_t i = index; (i & 1u) != 0; i >>= 1)
                    ++c;
                x1 ^= v1[c];
                x2 ^= v2[c];
                ++index;
            }

        private:
            uint32_t v1[32], v2[32], x1, x2, index;
    };

    // A fixed-seed generator (PCG32) for the frequency checks: a low-discrepancy sequence is too even for a chi-square
    class Pcg {
        public:
            explicit Pcg(uint64_t seed) : state(0), inc((seed << 1u) | 1u) {
                next32();
                state += seed;
                next32();
            }
            uint32_t next32() {
                const uint64_t old = state;
                state = old * 6364136223846793005ULL + inc;
                const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
                const uint32_t rot = static_cast<uint32_t>(old >> 59u);
                return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
            }
            double next() { return next32() * (1.0 / 4294967296.0); }

        private:
            uint64_t state, inc;
    };

    // ---- the ray-context double ------------------------------------------------------------------------------------------
    class Context : public IRayContext {
        public:
            Context() {
                memoryInit(memory);
                for (int i = 0; i < kVaryings; i++)
                    varying[i] = new float[kPoints * 4]();
            }
            ~Context() override {
                for (int i = 0; i < kVaryings; i++)
                    delete[] varying[i];
                memoryTini(memory);
            }
            int threadIndex() const override { return 0; }
            CMemPage *&scratch() override { return memory; }
            float **varyings() override { return varying; }
            void shade(CSurface *, int, int, EShadingDim, unsigned int) override {}
            void displace(CSurface *, int, int, EShadingDim, unsigned int) override {}
            void *saveShadingState() override { return nullptr; }
            void restoreShadingState(void *) override {}

            static const int kVaryings = 64, kPoints = 4096;
            float *varying[kVaryings];
            CMemPage *memory = nullptr;
    };

    // ---- a light and its roots ---------------------------------------------------------------------------------------------
    class StubLight : public CShaderGroupRefTarget {
        public:
            StubLight() = default;
    };

    // One bound area light of `name` and its registry; roots are recorded in call order
    class Rig {
        public:
            explicit Rig(const char *name = "al-test") : registry(16, EAreaLightMode::Full) { registry.declareLight(&light, 0, EAreaLightMode::Unset, name); }
            // `root->attributes` carries the light (the core sets the slot before an object is created in the scope)
            void add(CObject *root) {
                root->attributes->areaLight = &light;
                registry.record(root);
            }
            const CEmitterSet &set() const { return *registry.find(&light); }
            Context context;
            StubLight light;
            CEmitterRegistry registry;
    };

    // ---- geometry helpers ---------------------------------------------------------------------------------------------------
    // A planar polygon mesh of one face from the vertices (x, y, 0) given in order, placed at camera depth z
    inline CObject *polygon(const contract::Placement &pl, const std::vector<std::array<float, 2>> &xy) {
        const int n = static_cast<int>(xy.size());
        float *v = new float[3 * n];
        int *verts = new int[n];
        for (int i = 0; i < n; i++) {
            v[3 * i + 0] = xy[i][0];
            v[3 * i + 1] = xy[i][1];
            v[3 * i + 2] = 0.0f;
            verts[i] = i;
        }
        int nloops = 1, nverts = n;
        CObject *mesh = new CPolygonMesh(pl.attributes, pl.xform, contract::positionList(n, v), 1, &nloops, &nverts, verts);
        delete[] verts;
        return mesh;
    }

    // ---- the checks ---------------------------------------------------------------------------------------------------------
    inline bool nearRel(double a, double b, double tol) { return std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b)); }

    // The set's total area against the expected one
    inline void checkAreaMatches(const CEmitterSet &set, Context &ctx, double expected, double relTol = kAreaRelTol) {
        set.resolve(ctx);
        CHECK(nearRel(set.totalArea(), expected, relTol));
    }

    // Every draw's density is 1 / totalArea (the exact samplers), and the mean of 1 / pdfA over the draws is totalArea
    inline void checkDensity(const CEmitterSet &set, Context &ctx) {
        set.resolve(ctx);
        const double total = set.totalArea();
        CHECK(total > 0.0);
        Sobol2D sobol;
        double sumInv = 0.0;
        int bad = 0;
        const int n = 4096;
        for (int i = 0; i < n; i++) {
            double a, b;
            sobol.next(a, b);
            CAreaSample s;
            set.sample(a, b, s);
            if (!(s.pdfA > 0.0f) || !std::isfinite(s.pdfA) || !nearRel(1.0 / s.pdfA, total, kDensityRelTol))
                ++bad;
            sumInv += 1.0 / s.pdfA;
        }
        CHECK(bad == 0);
        CHECK(nearRel(sumInv / n, total, kDensityRelTol));
    }

    // The mean of 10^5 Sobol draws is the area-weighted centroid
    inline void checkCentroid(const CEmitterSet &set, Context &ctx, const double expected[3], double size, double tol = kCentroidTol) {
        set.resolve(ctx);
        Sobol2D sobol;
        double sum[3] = {0, 0, 0};
        for (int i = 0; i < kNumDraws; i++) {
            double a, b;
            sobol.next(a, b);
            CAreaSample s;
            set.sample(a, b, s);
            for (int k = 0; k < 3; k++)
                sum[k] += s.P[k];
        }
        for (int k = 0; k < 3; k++)
            CHECK(std::fabs(sum[k] / kNumDraws - expected[k]) <= tol * size);
    }

    // Leaves are hit in proportion to their area: chi-square over 10^5 fixed-seed draws, the degrees of freedom being the
    // number of leaves with area > 0 minus one (it must be a tabulated count)
    inline void checkFaceFrequencies(const CEmitterSet &set, Context &ctx) {
        set.resolve(ctx);
        std::map<const CSurface *, int> index;
        std::vector<double> area;
        for (int i = 0; i < set.numLeaves(); i++) {
            index[set.leafSurface(i)] = static_cast<int>(area.size());
            area.push_back(set.leafArea(i));
        }
        std::vector<long> hits(area.size(), 0);
        Pcg rng(0x5eed57ULL);
        for (int i = 0; i < kNumDraws; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            auto it = index.find(s.leaf);
            CHECK(it != index.end()); // a draw returns a leaf of the set
            if (it != index.end())
                ++hits[it->second];
        }
        double chi = 0.0;
        int cells = 0;
        for (size_t i = 0; i < area.size(); i++) {
            if (!(area[i] > 0.0)) {
                CHECK(hits[i] == 0); // never chosen
                continue;
            }
            const double expected = kNumDraws * area[i] / set.totalArea();
            const double d = hits[i] - expected;
            chi += d * d / expected;
            ++cells;
        }
        const double critical = chiSquareCritical(cells - 1);
        std::printf("  chi-square %.3f (%d dof, critical %.3f)\n", chi, cells - 1, critical); // measurement, never an assertion
        CHECK(critical > 0.0);
        CHECK(chi <= critical);
    }

    // A leaf of zero area (or no leaf at all) is never returned, and the density stays finite
    inline void checkZeroAreaNeverChosen(const CEmitterSet &set, Context &ctx) {
        set.resolve(ctx);
        std::map<const CSurface *, double> area;
        for (int i = 0; i < set.numLeaves(); i++)
            area[set.leafSurface(i)] = set.leafArea(i);
        Sobol2D sobol;
        Pcg rng(0x2e40ULL);
        for (int i = 0; i < 20000; i++) {
            double a, b;
            if (i % 2 == 0)
                sobol.next(a, b);
            else {
                a = rng.next();
                b = rng.next();
            }
            CAreaSample s;
            set.sample(a, b, s);
            if (set.totalArea() > 0.0) {
                CHECK(std::isfinite(s.pdfA) && s.pdfA > 0.0f);
                auto it = area.find(s.leaf);
                CHECK(it != area.end() && it->second > 0.0);
            }
            else {
                CHECK(s.pdfA == 0.0f && s.leaf == nullptr); // an empty set: no point
            }
        }
    }

    // The same 2 * 10^4 pairs drawn by one thread and by four threads agree bit for bit
    inline void checkThreadIdentity(const CEmitterSet &set, Context &ctx, int one = 1, int many = 4) {
        set.resolve(ctx);
        const int n = 20000;
        std::vector<double> u1(n), u2(n);
        Pcg rng(0x7bed1ULL);
        for (int i = 0; i < n; i++) {
            u1[i] = rng.next();
            u2[i] = rng.next();
        }
        auto draw = [&](int threads) {
            std::vector<CAreaSample> out(n);
            std::vector<std::thread> pool;
            for (int t = 0; t < threads; t++)
                pool.emplace_back([&, t] {
                    for (int i = t; i < n; i += threads)
                        set.sample(u1[i], u2[i], out[i]);
                });
            for (std::thread &th : pool)
                th.join();
            return out;
        };
        const std::vector<CAreaSample> a = draw(one), b = draw(many);
        int differ = 0;
        for (int i = 0; i < n; i++) {
            const bool same = a[i].leaf == b[i].leaf && std::memcmp(a[i].P, b[i].P, sizeof(a[i].P)) == 0 && std::memcmp(a[i].Ng, b[i].Ng, sizeof(a[i].Ng)) == 0 &&
                              std::memcmp(&a[i].u, &b[i].u, sizeof(float)) == 0 && std::memcmp(&a[i].v, &b[i].v, sizeof(float)) == 0 &&
                              std::memcmp(&a[i].pdfA, &b[i].pdfA, sizeof(float)) == 0;
            if (!same)
                ++differ;
        }
        CHECK(differ == 0);
    }

    // P(u, v) of the drawn leaf equals the drawn point (the exact samplers: planar triangles and quads)
    inline void checkRoundTrip(const CEmitterSet &set, Context &ctx, double tol = kRoundTripTol) {
        set.resolve(ctx);
        Pcg rng(0x40bdULL);
        int bad = 0;
        for (int i = 0; i < 10000; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            if (s.leaf == nullptr) {
                ++bad;
                continue;
            }
            float P[3], Ng[3];
            s.leaf->samplePoint(s.u, s.v, P, Ng);
            const double d = std::sqrt((double)(P[0] - s.P[0]) * (P[0] - s.P[0]) + (double)(P[1] - s.P[1]) * (P[1] - s.P[1]) + (double)(P[2] - s.P[2]) * (P[2] - s.P[2]));
            if (!(d <= tol) || s.u < -1e-6f || s.u > 1.000001f || s.v < -1e-6f || s.v > 1.000001f)
                ++bad;
        }
        CHECK(bad == 0);
    }

    // A frame the geometry reads (the contract programs do the same)
    inline void installFrame() {
        static CGeometryFrame frame;
        frame = CGeometryFrame();
        frame.numThreads = 1;
        frame.maxGridSize = 256;
        frame.maxEyeSplits = 10;
        frame.projection = OPTIONS_PROJECTION_PERSPECTIVE;
        frame.imagePlane = 1.0f;
        frame.dPixeldx = 2.0f / 640.0f;
        frame.dPixeldy = 2.0f / 480.0f;
        frame.clipMin = 0.01f;
        installGeometryFrame(&frame);
    }

} // namespace alsampling

#endif
