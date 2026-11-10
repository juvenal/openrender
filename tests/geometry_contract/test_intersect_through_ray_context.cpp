/**
 * Project: openRender
 *
 * File: test_intersect_through_ray_context.cpp
 *
 * Description:
 *   Geometry_IntersectThroughRayContext: a program that links only libRIBVector and libRIBCommon intersects a sphere, a polygon mesh (lazy materialize) and a displaced quadric (tessellation through displace) through a recording IRayContext; shade delivers the hits into the double's varyings.
 *
 *   Written first for spec 031 (tests/geometry_contract/, label geometry_contract); red on the starting tree
 *   because the interface it exercises does not exist yet (contracts/tests.md). Built only while
 *   OPENRENDER_031_RED_TESTS is ON until its phase enables it.
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <cmath>
#include <vector>

#include "contract_test.h"
#include "fixtures.h"
#include "memory.h"
#include "object.h"
#include "polygons.h"
#include "rayContext.h"
#include "ray.h"
#include "stateHandles.h"
#include "surface.h"

namespace {

    class StubShader : public CShaderGroupRefTarget {
        public:
            StubShader() = default;
    };

    // The thread context of the intersection: no shader runs, displace() is recorded and leaves P as sampled.
    class RecordingRayContext : public IRayContext {
        public:
            RecordingRayContext() {
                memoryInit(memory);
                for (int i = 0; i < kVaryings; i++)
                    varying[i] = new float[kPoints * 4]();
            }
            ~RecordingRayContext() override {
                for (int i = 0; i < kVaryings; i++)
                    delete[] varying[i];
                memoryTini(memory);
            }
            int threadIndex() const override { return 0; }
            CMemPage *&scratch() override { return memory; }
            float **varyings() override { return varying; }
            void shade(CSurface *, int, int, EShadingDim, unsigned int) override { ++shades; }
            void displace(CSurface *, int, int, EShadingDim, unsigned int) override { ++displaces; }
            void *saveShadingState() override { return nullptr; }
            void restoreShadingState(void *) override {}

            static const int kVaryings = 64, kPoints = 4096;
            float *varying[kVaryings];
            CMemPage *memory = nullptr;
            int shades = 0, displaces = 0;
    };

    void installFrame() {
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

    // a ray along +z at (x, y)
    CRay makeRay(float x, float y) {
        CRay ray;
        ray = CRay();
        ray.from[0] = x;
        ray.from[1] = y;
        ray.from[2] = 0;
        ray.dir[0] = 0;
        ray.dir[1] = 0;
        ray.dir[2] = 1;
        ray.time = 0;
        ray.flags = 0;
        ray.t = 1e30f;
        ray.tmin = 1e-6f;
        ray.object = nullptr;
        ray.invDir[0] = 1e30;
        ray.invDir[1] = 1e30;
        ray.invDir[2] = 1;
        return ray;
    }

    void testSphere(RecordingRayContext &ctx) {
        contract::Placement placement(5.0f);
        CSurface *sphere = contract::sphere(placement);
        CRay ray = makeRay(0, 0);
        sphere->intersect(ctx, &ray);
        CHECK(ray.object == sphere);
        CHECK(std::fabs(ray.t - 4.0f) < 1e-3f);

        // the hit is delivered through the context's varyings: u, v, time, I and du
        CRay *rays[1] = {&ray};
        sphere->shade(ctx, 1, rays);
        const float *u = ctx.varyings()[VARIABLE_U];
        const float *v = ctx.varyings()[VARIABLE_V];
        const float *time = ctx.varyings()[VARIABLE_TIME];
        const float *I = ctx.varyings()[VARIABLE_I];
        const float *du = ctx.varyings()[VARIABLE_DU];
        CHECK(u[0] == ray.u && v[0] == ray.v);
        CHECK(time[0] == ray.time);
        CHECK(I[0] == ray.dir[0] && I[1] == ray.dir[1] && I[2] == ray.dir[2]);
        CHECK(du != nullptr);

        // a ray that misses the sphere reports nothing
        CRay miss = makeRay(3.0f, 0);
        sphere->intersect(ctx, &miss);
        CHECK(miss.object == nullptr);
    }

    void testPolygonMesh(RecordingRayContext &ctx) {
        contract::Placement placement(5.0f);
        float *v = new float[12]{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
        int nloops = 1, nverts = 4, verts[4] = {0, 1, 2, 3};
        CPolygonMesh *mesh = new CPolygonMesh(placement.attributes, placement.xform, contract::positionList(4, v), 1, &nloops, &nverts, verts);
        mesh->materialize(ctx); // lazy child creation reaches the thread's context only through IRayContext
        CRay ray = makeRay(0.25f, 0.25f);
        mesh->intersect(ctx, &ray);
        CHECK(ray.object != nullptr);
        CHECK(std::fabs(ray.t - 5.0f) < 1e-3f);
    }

    void testDisplacedQuadric(RecordingRayContext &ctx) {
        contract::Placement placement(5.0f);
        placement.attributes->bindDisplacement(ShaderGroupRef::adopt(new StubShader));
        placement.attributes->maxDisplacement = 0.1f;
        CSurface *sphere = contract::sphere(placement);
        CRay ray = makeRay(0, 0);
        sphere->intersect(ctx, &ray);
        // tessellation of a displaced surface asks the context to displace, and the hit lands on a piece of it
        CHECK(ctx.displaces > 0);
        CHECK(ray.object != nullptr);
    }

} // namespace

int main() {
    CRiStandardSession session;
    installFrame();
    {
        RecordingRayContext ctx;
        testSphere(ctx);
        testPolygonMesh(ctx);
        testDisplacedQuadric(ctx);
    }
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("Geometry_IntersectThroughRayContext");
}
