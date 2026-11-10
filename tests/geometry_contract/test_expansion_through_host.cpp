/**
 * Project: openRender
 *
 * File: test_expansion_through_host.cpp
 *
 * Description:
 *   Geometry_ExpansionThroughHost: a delayed object and a delayed instance expand once when intersected from two threads and diced from a third, through a recording CGeometryHost; with no host installed exactly one CODE_BUG is reported and the object stays unexpanded.
 *
 *   Written first for spec 031 (tests/geometry_contract/, label geometry_contract); red on the starting tree
 *   because the interface it exercises does not exist yet (contracts/tests.md). Built only while
 *   OPENRENDER_031_RED_TESTS is ON until its phase enables it.
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "contract_test.h"
#include "delayed.h"
#include "common/diagnostics.h"
#include "dicer.h"
#include "fixtures.h"
#include "geometryHost.h"
#include "ray.h"
#include "rayContext.h"

namespace {

    // Counts the CODE_BUG reports.
    class CountingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode code, EDiagnosticSeverity, const char *) override {
                if (code == CODE_BUG)
                    ++bugs;
            }
            std::atomic<int> bugs{0};
    };

    // Serializes like the core: one lock, the processed flag tested again under it.
    class RecordingHost : public CGeometryHost {
        public:
            CObject *capture(const CObject *, CAttributes *, CXform *) override { return nullptr; }
            CObject *expand(const CObject *, CAttributes *) override { return nullptr; }
            void releaseAtFrameEnd(CFrameResource *) override {}
            int findCoordinateSystem(const char *, const float *&, const float *&, ECoordinateSystem &) override { return FALSE; }

            void expandDelayedObject(CDelayedObject *object) override {
                std::lock_guard<std::mutex> lock(mutex);
                if (object->processed == FALSE) {
                    ++objectExpansions;
                    object->processed = TRUE;
                }
            }
            void expandDelayedInstance(CDelayedInstance *instance) override {
                std::lock_guard<std::mutex> lock(mutex);
                if (instance->processed == FALSE) {
                    ++instanceExpansions;
                    instance->processed = TRUE;
                }
            }

            std::mutex mutex;
            std::atomic<int> objectExpansions{0}, instanceExpansions{0};
    };

    class NullRayContext : public IRayContext {
        public:
            int threadIndex() const override { return 0; }
            CMemPage *&scratch() override { return memory; }
            float **varyings() override { return nullptr; }
            void shade(CSurface *, int, int, EShadingDim, unsigned int) override {}
            void displace(CSurface *, int, int, EShadingDim, unsigned int) override {}
            void *saveShadingState() override { return nullptr; }
            void restoreShadingState(void *) override {}
            CMemPage *memory = nullptr;
    };

    class NullDicer : public IDicer {
        public:
            IRayContext &rayContext() override { return context; }
            void drawObject(CObject *) override {}
            void drawGrid(CSurface *, int, int, float, float, float, float, const CGridSnap *) override {}
            void drawPoints(CSurface *, int) override {}
            void probe(CSurface *, int, int, EShadingDim, unsigned int) override {}
            int inFrustum(const float *, const float *) override { return TRUE; }
            NullRayContext context;
    };

    // A delayed object whose expansion the host performs; the data and functions are never touched by Geometry.
    CDelayedObject *makeDelayedObject(const contract::Placement &p) {
        const float bmin[3] = {-1, -1, 4}, bmax[3] = {1, 1, 6};
        return new CDelayedObject(p.attributes, p.xform, bmin, bmax, nullptr, nullptr, nullptr);
    }

    template <class T> void threeThreads(T *object) {
        std::thread a([&] { NullRayContext ctx; CRay ray; ray = CRay(); object->intersect(ctx, &ray); });
        std::thread b([&] { NullRayContext ctx; CRay ray; ray = CRay(); object->intersect(ctx, &ray); });
        std::thread c([&] { NullDicer dicer; object->dice(dicer); });
        a.join();
        b.join();
        c.join();
    }

    void testExpandsOnce() {
        contract::Placement placement(5.0f);
        RecordingHost host;
        setGeometryHost(&host);

        CDelayedObject *object = makeDelayedObject(placement);
        threeThreads(object);
        CHECK(host.objectExpansions == 1);
        CHECK(object->processed == TRUE);

        CObject *inner = contract::sphere(placement);
        CDelayedInstance *instance = new CDelayedInstance(placement.attributes, placement.xform, inner);
        threeThreads(instance);
        CHECK(host.instanceExpansions == 1);
        CHECK(instance->processed == TRUE);

        setGeometryHost(nullptr);
    }

    void testNoHost() {
        contract::Placement placement(5.0f);
        setGeometryHost(nullptr);
        CountingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        CDelayedObject *object = makeDelayedObject(placement);
        NullRayContext ctx;
        CRay ray;
        ray = CRay();
        object->intersect(ctx, &ray);
        NullDicer dicer;
        object->dice(dicer);
        installDiagnosticSink(previous);
        // the object stays unexpanded, and the missing host is reported once, not once per call
        CHECK(object->processed == FALSE);
        CHECK(sink.bugs == 1);
    }

} // namespace

int main() {
    CRiStandardSession session;
    CGeometryFrame frame;
    frame = CGeometryFrame();
    frame.numThreads = 3;
    installGeometryFrame(&frame);
    testExpandsOnce();
    testNoHost();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("Geometry_ExpansionThroughHost");
}
