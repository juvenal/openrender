/**
 * tests/shading_contract/case_hider_tracer.cpp
 *
 * HiderTracer_PerContext (spec 029, T036; T-T1; FR-016, US6 AS2, SC-007). Links ribRender: it constructs real hiders.
 *
 * Each hider owns its shading context and is the ITracer of the context it owns (CHider, hider.h): there is no frame-wide
 * tracer slot any more. Two hiders, a CRaytracer and a CPhotonHider, are built in one RiBegin session, each with its own
 * context, and:
 *   - the two contexts are two objects; each context's tracer() is its own hider, as an ITracer, and no other;
 *   - a ray counted through one context (countTracedRay(), what CHider::traceRay() calls) counts on that context only
 *     (tracedRays(), the read-only counter this case asks of the context, like vertexMemoryInUse());
 *   - a context with no hider and no tracer installed, whose services offer none either, warns once per kind and treats
 *     the rays as misses: the bundle forms, gather() and the probe forms degrade, they do not crash.
 * The body of traceRay()/traceBundle() is the starting tree's, moved without a change of expression; the whole visual suite
 * (byte-identical output) and the trace replay are its guards, so this case does not trace through a world.
 *
 * Red on the starting tree: a build failure (there is no CHider, hider.h, shadingContext() or tracedRays(); a hider is a
 * CShadingContext).
 */
#include "cases_029.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "hider.h"
#include "photon.h"
#include "raytracer.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"
#include "ri/state/camera.h"

using namespace openrender::shading;

namespace {

    // The raytrace hider wants a camera it never asks anything of here
    class CStubCamera final : public openrender::ICamera {
        public:
            openrender::CameraRay generateRay(float, float, float, const float[2]) const override {
                openrender::CameraRay r = {};
                r.dir[2] = 1;
                return r;
            }
    };

} // namespace

int caseHiderTracer() {
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);

    // ---- a context with no hider and no tracer: warns once per kind, treats the rays as misses -------------------------
    {
        CContractServices svc;
        svc.tracerValue = nullptr;
        CHarness h(svc, 4);
        CT_EXPECT(h.context->tracer() == nullptr); // nothing of its own, and the services have none
        printf("  no hider, no tracer: cm_visibility\n");
        CProgrammableShaderInstance *s = h.load("cm_visibility", false);
        CT_EXPECT(s != nullptr);
        if (s != nullptr) {
            const SHarnessResult r = h.runSurface(s);
            for (int i = 0; i < h.numPoints; i++) {
                const float v = r.ciAt(i, 0);
                CT_EXPECT(v == 0.0f || v == 1.0f); // a miss: the call still answers
            }
        }
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 1); // one warning for the kind, not four
    }

    // ---- two hiders, each with its own context ---------------------------------------------------------------------------
    RiBegin(RI_NULL);
    RiWorldBegin();
    {
        CStubCamera camera;
        CRaytracer *raytracer = new CRaytracer(0, camera);
        CPhotonHider *photon = new CPhotonHider(1, CRenderer::context->getAttributes(TRUE));
        CHider *rt = raytracer;
        CHider *ph = photon;
        CShadingContext &a = rt->shadingContext();
        CShadingContext &b = ph->shadingContext();

        printf("  two hiders, two contexts\n");
        CT_EXPECT(&a != &b);
        CT_EXPECT(a.tracer() == static_cast<ITracer *>(rt));
        CT_EXPECT(b.tracer() == static_cast<ITracer *>(ph));
        CT_EXPECT(a.tracer() != b.tracer());
        CT_EXPECT(a.rayDepth() == 0 && b.rayDepth() == 0);

        printf("  a ray counts on its own context only\n");
        CT_EXPECT(a.tracedRays() == 0 && b.tracedRays() == 0);
        a.countTracedRay();
        a.countTracedRay();
        b.countTracedRay();
        CT_EXPECT(a.tracedRays() == 2);
        CT_EXPECT(b.tracedRays() == 1);

        delete ph;
        delete rt;
    }
    RiWorldEnd();
    RiEnd();

    installDiagnosticSink(previous);
    return contract_test::finish();
}
