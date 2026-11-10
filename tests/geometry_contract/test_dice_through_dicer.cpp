/**
 * Project: openRender
 *
 * File: test_dice_through_dicer.cpp
 *
 * Description:
 *   Geometry_DiceThroughDicer: a program that links only libRIBVector and libRIBCommon dices a sphere, a bilinear patch, a trimmed NURBS patch and a points primitive through a recording IDicer; no hider is involved.
 *
 *   Written first for spec 031 (tests/geometry_contract/, label geometry_contract); red on the starting tree
 *   because the interface it exercises does not exist yet (contracts/tests.md). Built only while
 *   OPENRENDER_031_RED_TESTS is ON until its phase enables it.
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <cstring>
#include <vector>

#include "contract_test.h"
#include "dicer.h"
#include "fixtures.h"
#include "memory.h"
#include "object.h"
#include "surface.h"

namespace {

    // Answers the probe the way a hider with no shader does: the undisplaced surface sampled at the written points.
    class RecordingRayContext : public IRayContext {
        public:
            RecordingRayContext() {
                memoryInit(memory);
                for (int i = 0; i < kVaryings; i++)
                    varying[i] = new float[kPoints * 4]();
                locals = new float **[kLocals]();
            }
            ~RecordingRayContext() override {
                for (int i = 0; i < kVaryings; i++)
                    delete[] varying[i];
                delete[] locals;
                memoryTini(memory);
            }
            int threadIndex() const override { return 0; }
            CMemPage *&scratch() override { return memory; }
            float **varyings() override { return varying; }
            void shade(CSurface *, int, int, EShadingDim, unsigned int) override {}
            void displace(CSurface *, int, int, EShadingDim, unsigned int) override {}
            void *saveShadingState() override { return nullptr; }
            void restoreShadingState(void *) override {}

            static const int kVaryings = 64, kLocals = 64, kPoints = 4096;
            float *varying[kVaryings];
            float ***locals;
            CMemPage *memory = nullptr;
    };

    struct GridCall {
        int udiv, vdiv;
        float umin, umax, vmin, vmax;
    };

    class RecordingDicer : public IDicer {
        public:
            IRayContext &rayContext() override { return context; }
            void drawObject(CObject *object) override { objects.push_back(object); }
            void drawGrid(CSurface *, int udiv, int vdiv, float umin, float umax, float vmin, float vmax, const CGridSnap *) override {
                grids.push_back({udiv, vdiv, umin, umax, vmin, vmax});
            }
            void drawPoints(CSurface *, int n) override { points.push_back(n); }
            void probe(CSurface *surface, int u, int v, EShadingDim, unsigned int up) override {
                ++probes;
                surface->sample(0, u * v, context.varying, context.locals, up);
            }
            int inFrustum(const float *, const float *) override { return TRUE; }

            RecordingRayContext context;
            std::vector<GridCall> grids;
            std::vector<CObject *> objects;
            std::vector<int> points;
            int probes = 0;
    };

    // dice `surface`, then keep dicing the pieces it queued until only grids are left (bounded)
    void diceAll(CSurface *surface, RecordingDicer &d) {
        surface->dice(d);
        for (size_t i = 0; i < d.objects.size() && i < 256; i++)
            d.objects[i]->dice(d);
    }

    void installFrame(int maxGridSize) {
        static CGeometryFrame frame; // value-initialised: only the fields that survive the lens phase are set
        frame = CGeometryFrame();
        frame.numThreads = 1;
        frame.maxGridSize = maxGridSize;
        frame.maxEyeSplits = 10;
        frame.projection = OPTIONS_PROJECTION_PERSPECTIVE;
        frame.imagePlane = 1.0f;
        frame.dPixeldx = 2.0f / 640.0f;
        frame.dPixeldy = 2.0f / 480.0f;
        frame.pixelLeft = 0;
        frame.pixelTop = 0;
        frame.clipMin = 0.01f;
        installGeometryFrame(&frame);
    }

    void checkGrids(const RecordingDicer &d, int maxGridSize) {
        CHECK(!d.grids.empty());
        for (const GridCall &g : d.grids) {
            CHECK(g.udiv >= 1 && g.vdiv >= 1);
            CHECK((g.udiv + 1) * (g.vdiv + 1) <= maxGridSize);
            CHECK(g.umin >= 0.0f && g.umax <= 1.0f + 1e-6f && g.umin < g.umax);
            CHECK(g.vmin >= 0.0f && g.vmax <= 1.0f + 1e-6f && g.vmin < g.vmax);
        }
    }

    void testQuadricAndPatches() {
        const int maxGridSize = 256;
        installFrame(maxGridSize);
        contract::Placement placement(5.0f);

        RecordingDicer sphere;
        diceAll(contract::sphere(placement), sphere);
        checkGrids(sphere, maxGridSize);
        CHECK(sphere.probes > 0); // the displacement probe went through the double

        RecordingDicer patch;
        diceAll(contract::bilinearPatch(placement), patch);
        checkGrids(patch, maxGridSize);

        RecordingDicer trimmed;
        diceAll(contract::trimmedNurbsPatch(placement), trimmed);
        CHECK(!trimmed.grids.empty() || !trimmed.objects.empty()); // a trimmed patch is diced or split, never dropped silently

        // a coarser shading rate asks for no more grids than a finer one asks for (the split count follows the rate)
        RecordingDicer fine;
        contract::Placement finePlacement(5.0f);
        finePlacement.attributes->shadingRate = 0.25f;
        diceAll(contract::sphere(finePlacement), fine);
        CHECK(fine.grids.size() >= sphere.grids.size());
        checkGrids(fine, maxGridSize);
    }

    void testPoints() {
        installFrame(256);
        contract::Placement placement(5.0f);
        RecordingDicer d;
        diceAll(contract::points(placement, 16), d);
        CHECK(d.points.size() == 1);
        CHECK(d.points.empty() || d.points[0] == 16);
        CHECK(d.grids.empty());
    }

} // namespace

int main() {
    CRiStandardSession session;
    testQuadricAndPatches();
    testPoints();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("Geometry_DiceThroughDicer");
}
