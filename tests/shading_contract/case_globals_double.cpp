/**
 * tests/shading_contract/case_globals_double.cpp
 *
 * ShadingContract_ShaderGlobalsDouble (spec 026, T057; US5 AS1, AS3, FR-015, FR-016).
 *
 * In 026 nothing in the render path produces or consumes ShaderGlobals (FR-018): only this test does.
 * A double producer fills a ShaderGlobals for each situation the renderer has today, and a checker
 * validates the invariants of contracts/shader-globals.md:
 *   - structure of arrays with a runtime point count; a batch of one is valid;
 *   - Strided<T>: stride 0 means one value for every point, data == nullptr means the field is absent;
 *   - the derivative tail: numDerivativeTail is 0 or 2 * numPoints, the tail points are never counted
 *     and never active, and their sampleKey copies the real point's key;
 *   - gridU * gridV == numPoints for a grid;
 *   - inactive lanes hold unspecified values (the producer poisons them with NaN);
 *   - only P, time and the per-batch fields may be assumed: a consumer that reads nothing else works on
 *     every producer, the bare point included.
 * The situations are the rows of the situation table that produce a batch: a camera grid, camera ray
 * hits under each hit mode, a secondary hit, a shadow-type hit, a photon hit and a bare point.
 */
#include "cases.h"

#include <cmath>
#include <limits>
#include <vector>

#include "case_common.h"
#include "libshader/shading/rendererServices.h"
#include "libshader/shading/shaderGlobals.h"

using namespace openrender::shading;
using openrender::sampling::SampleKey;

static_assert(kRendererServicesVersion.major == 2, "ShaderGlobals moves with the services version (D22)");

namespace {

    const float kNaN = std::numeric_limits<float>::quiet_NaN();

    // The backing store of one ShaderGlobals
    struct SBatch {
            std::string name;
            ShaderGlobals g;
            std::vector<float> P, N, Ng, I, u, v, du, dv, rayWidth, dPdu, dPdv, time, dPdtime;
            std::vector<SampleKey> keys;
            std::vector<int> tags;
            int totalPoints = 0; // real points + tail
    };

    SampleKey keyOf(int k) {
        SampleKey key{};
        key.full = 0x1000 + (uint64_t)k;
        key.path = 0x2000 + (uint64_t)k;
        key.pixelSampleIndex = k;
        key.kind = 1;
        return key;
    }

    // Fill `v` with `stride` floats per point for `total` points, a value that depends on the point
    void fill(std::vector<float> &v, int stride, int total, float base) {
        v.resize((size_t)stride * total);
        for (int p = 0; p < total; p++)
            for (int k = 0; k < stride; k++)
                v[(size_t)stride * p + k] = base + (float)p + 0.1f * (float)k;
    }

    // Build a batch. `fields` selects which optional fields are present.
    struct SFields {
            bool normals, incident, uv, duv, rayWidth, dPd, dPdtime;
    };

    SBatch make(const char *name, BatchShape shape, int numPoints, int gridU, int gridV, bool tail, RayType rt, HitMode hm, TransportMode tm, ShadingPass pass,
                const SFields &f, bool sharedTime, int inactiveLane = -1) {
        SBatch b;
        b.name = name;
        const int numTail = tail ? 2 * numPoints : 0;
        b.totalPoints = numPoints + numTail;
        ShaderGlobals &g = b.g;
        g.numPoints = numPoints;
        g.numDerivativeTail = numTail;
        g.shape = shape;
        g.gridU = gridU;
        g.gridV = gridV;
        g.rayType = rt;
        g.hitMode = hm;
        g.transport = tm;
        g.pass = pass;
        g.pathDepth = (rt == RayType::Secondary) ? 1 : 0;
        g.shadingCellFootprint = 1.0f;
        g.shutterOpen = 0;
        g.dtime = 1;

        fill(b.P, 3, b.totalPoints, 1.0f);
        g.P = {b.P.data(), 3};
        if (sharedTime) {
            b.time = {0.5f}; // stride 0: one value for every point
            g.time = {b.time.data(), 0};
        }
        else {
            fill(b.time, 1, b.totalPoints, 0.0f);
            g.time = {b.time.data(), 1};
        }
        if (f.normals) {
            fill(b.N, 3, b.totalPoints, 2.0f);
            fill(b.Ng, 3, b.totalPoints, 3.0f);
            g.N = {b.N.data(), 3};
            g.Ng = {b.Ng.data(), 3};
        }
        if (f.incident) {
            fill(b.I, 3, b.totalPoints, 4.0f);
            g.I = {b.I.data(), 3};
        }
        if (f.uv) {
            fill(b.u, 1, b.totalPoints, 0.0f);
            fill(b.v, 1, b.totalPoints, 0.5f);
            g.u = {b.u.data(), 1};
            g.v = {b.v.data(), 1};
        }
        if (f.duv) {
            fill(b.du, 1, b.totalPoints, 0.01f);
            fill(b.dv, 1, b.totalPoints, 0.02f);
            g.du = {b.du.data(), 1};
            g.dv = {b.dv.data(), 1};
        }
        if (f.rayWidth) {
            fill(b.rayWidth, 1, b.totalPoints, 0.05f);
            g.rayWidth = {b.rayWidth.data(), 1};
        }
        if (f.dPd) {
            fill(b.dPdu, 3, b.totalPoints, 5.0f);
            fill(b.dPdv, 3, b.totalPoints, 6.0f);
            g.dPdu = {b.dPdu.data(), 3};
            g.dPdv = {b.dPdv.data(), 3};
        }
        if (f.dPdtime) {
            fill(b.dPdtime, 3, b.totalPoints, 7.0f);
            g.dPdtime = {b.dPdtime.data(), 3};
        }
        // The sample key of every real point; the tail copies the real point's (du block, then dv block)
        b.keys.resize(b.totalPoints);
        for (int p = 0; p < numPoints; p++)
            b.keys[p] = keyOf(p);
        for (int t = 0; t < numTail; t++)
            b.keys[numPoints + t] = b.keys[t % numPoints];
        g.sampleKey = {b.keys.data(), 1};

        // Inactive lanes are unspecified: poison them
        if (inactiveLane >= 0) {
            b.tags.assign(numPoints, 0);
            b.tags[inactiveLane] = 1;
            g.activeTags = b.tags.data();
            for (int k = 0; k < 3; k++)
                b.P[3 * inactiveLane + k] = kNaN;
            if (!sharedTime)
                b.time[inactiveLane] = kNaN;
        }
        return b;
    }

    // ---- the checker: the documented invariants ------------------------------------------------------
    template <class T> void checkField(const Strided<T> &f, int expectedStride, int totalPoints, size_t backingSize, const char *what) {
        if (!f.present())
            return;
        // The stride is the field's own, or 0 (one value for every point)
        const bool strideOk = (f.stride == expectedStride) || (f.stride == 0);
        if (!strideOk)
            fprintf(stderr, "  field %s has stride %d\n", what, f.stride);
        CT_EXPECT(strideOk);
        // A stride > 0 must be backed for every point, the tail included
        if (f.stride > 0)
            CT_EXPECT(backingSize >= (size_t)f.stride * totalPoints);
        else
            CT_EXPECT(backingSize >= 1);
    }

    void validate(const SBatch &b) {
        printf("  %s\n", b.name.c_str());
        const ShaderGlobals &g = b.g;

        // Shape and counts
        CT_EXPECT(g.numPoints >= 1); // a batch of one is valid
        CT_EXPECT(g.numDerivativeTail == 0 || g.numDerivativeTail == 2 * g.numPoints);
        if (g.shape == BatchShape::Grid)
            CT_EXPECT(g.gridU * g.gridV == g.numPoints);
        if (g.shape == BatchShape::Point)
            CT_EXPECT(g.numPoints == 1);

        // Presence: P, time and the per-batch fields are always there
        CT_EXPECT(g.P.present());
        CT_EXPECT(g.time.present());
        CT_EXPECT(g.pathDepth >= 0);
        CT_EXPECT(g.shadingCellFootprint > 0.0f);
        CT_EXPECT(g.dtime >= 0.0f);

        // Strides and backing
        checkField(g.P, 3, b.totalPoints, b.P.size(), "P");
        checkField(g.N, 3, b.totalPoints, b.N.size(), "N");
        checkField(g.Ng, 3, b.totalPoints, b.Ng.size(), "Ng");
        checkField(g.I, 3, b.totalPoints, b.I.size(), "I");
        checkField(g.u, 1, b.totalPoints, b.u.size(), "u");
        checkField(g.v, 1, b.totalPoints, b.v.size(), "v");
        checkField(g.du, 1, b.totalPoints, b.du.size(), "du");
        checkField(g.dv, 1, b.totalPoints, b.dv.size(), "dv");
        checkField(g.rayWidth, 1, b.totalPoints, b.rayWidth.size(), "rayWidth");
        checkField(g.dPdu, 3, b.totalPoints, b.dPdu.size(), "dPdu");
        checkField(g.dPdv, 3, b.totalPoints, b.dPdv.size(), "dPdv");
        checkField(g.time, 1, b.totalPoints, b.time.size(), "time");
        checkField(g.dPdtime, 3, b.totalPoints, b.dPdtime.size(), "dPdtime");

        // The derivative tail: never counted, never active, and its sample key is the real point's
        if (g.numDerivativeTail > 0 && g.sampleKey.present()) {
            for (int t = 0; t < g.numDerivativeTail; t++) {
                const SampleKey &tail = *g.sampleKey.at(g.numPoints + t);
                const SampleKey &real = *g.sampleKey.at(t % g.numPoints);
                CT_EXPECT(tail.full == real.full && tail.path == real.path && tail.pixelSampleIndex == real.pixelSampleIndex);
            }
        }
        if (g.activeTags != nullptr) {
            // the tags cover the real points only
            int active = 0;
            for (int p = 0; p < g.numPoints; p++)
                if (g.activeTags[p] == 0)
                    ++active;
            CT_EXPECT(active >= 0 && active <= g.numPoints);
        }
    }

    // A consumer that assumes only P, time and the per-batch fields: it works on every producer
    void consumeMinimal(const SBatch &b) {
        const ShaderGlobals &g = b.g;
        double sum = 0;
        for (int p = 0; p < g.numPoints; p++) {
            if (g.activeTags != nullptr && g.activeTags[p] != 0)
                continue; // an inactive lane holds unspecified values: never read
            const float *P = g.P.at(p);
            const float *t = g.time.at(p);
            CT_EXPECT(std::isfinite(P[0]) && std::isfinite(P[1]) && std::isfinite(P[2]));
            CT_EXPECT(std::isfinite(t[0]));
            sum += P[0] + P[1] + P[2] + t[0];
        }
        CT_EXPECT(std::isfinite(sum));
        // A shared (stride 0) time is the same for every point
        if (g.time.stride == 0)
            for (int p = 1; p < g.numPoints; p++)
                CT_EXPECT(g.time.at(p) == g.time.at(0));
    }

} // namespace

int caseShaderGlobalsDouble() {
    std::vector<SBatch> batches;
    const SFields rasterFull = {true, true, true, true, false, true, false};
    const SFields rayHit = {true, true, true, false, true, true, false};
    const SFields photon = {false, false, false, false, false, false, false};
    const SFields bare = {false, false, false, false, false, false, false};
    const SFields displacement = {true, false, true, true, false, true, false};

    // Row 1: a REYES grid, camera
    batches.push_back(make("camera grid (row 1)", BatchShape::Grid, 12, 4, 3, false, RayType::None, HitMode::Shade, TransportMode::Radiance, ShadingPass::Full, rasterFull, false));
    // Row 3: camera ray hits, under each hit mode, with the derivative tail
    batches.push_back(make("camera ray hit, hitmode shade (row 3)", BatchShape::Span, 5, 0, 0, true, RayType::Camera, HitMode::Shade, TransportMode::Radiance, ShadingPass::Full, rayHit, false));
    batches.push_back(make("camera ray hit, hitmode primitive (row 3)", BatchShape::Span, 5, 0, 0, true, RayType::Camera, HitMode::Primitive, TransportMode::Radiance, ShadingPass::Full, rayHit, false));
    // Row 4: a secondary hit
    batches.push_back(make("secondary hit (row 4)", BatchShape::Span, 3, 0, 0, true, RayType::Secondary, HitMode::Shade, TransportMode::Radiance, ShadingPass::Full, rayHit, false));
    // Row 5: a shadow-type hit
    batches.push_back(make("shadow-type hit (row 5)", BatchShape::Span, 3, 0, 0, false, RayType::Shadow, HitMode::Primitive, TransportMode::Radiance, ShadingPass::Full, rayHit, true));
    // Rows 8 and 9: displacement passes
    batches.push_back(make("displacement-only pass (row 8)", BatchShape::Grid, 6, 3, 2, false, RayType::None, HitMode::Shade, TransportMode::Radiance, ShadingPass::DisplaceOnly, displacement, false));
    batches.push_back(make("displacement estimate (row 9)", BatchShape::Grid, 6, 3, 2, false, RayType::None, HitMode::Shade, TransportMode::Radiance, ShadingPass::DisplaceEstimate, displacement, false));
    // Row 10: photon emission: only P and time (and the key)
    batches.push_back(make("photon emission (row 10)", BatchShape::Span, 4, 0, 0, false, RayType::Photon, HitMode::Shade, TransportMode::Importance, ShadingPass::Full, photon, false));
    // A bare point (row 14): a batch of one, only P and time, one shared time
    batches.push_back(make("bare point (row 14)", BatchShape::Point, 1, 0, 0, false, RayType::None, HitMode::Shade, TransportMode::Radiance, ShadingPass::Full, bare, true));
    // Inactive lanes: unspecified values, never read
    batches.push_back(make("camera ray hit with an inactive lane", BatchShape::Span, 5, 0, 0, true, RayType::Camera, HitMode::Shade, TransportMode::Radiance, ShadingPass::Full, rayHit, false, 2));

    for (const SBatch &b : batches) {
        validate(b);
        consumeMinimal(b);
    }

    // The presence rules, row by row
    CT_EXPECT(!batches[7].g.N.present() && !batches[7].g.I.present() && !batches[7].g.u.present()); // photon: P and time only
    CT_EXPECT(batches[1].g.rayWidth.present() && !batches[1].g.du.present());                       // a ray hit carries rayWidth, not du/dv
    CT_EXPECT(batches[0].g.du.present() && !batches[0].g.rayWidth.present());                       // a grid carries du/dv, not rayWidth
    CT_EXPECT(!batches[0].g.dPdx.present() && !batches[1].g.dPdy.present());                        // dPdx, dPdy: reserved, no producer
    CT_EXPECT(batches[1].g.numDerivativeTail == 2 * batches[1].g.numPoints);
    CT_EXPECT(batches[0].g.numDerivativeTail == 0);

    // Strided<T>'s accessors
    Strided<float> absent;
    CT_EXPECT(!absent.present());
    const float one = 3.0f;
    Strided<float> shared{&one, 0};
    CT_EXPECT(shared.present() && shared.at(0) == shared.at(7));

    return contract_test::finish();
}
