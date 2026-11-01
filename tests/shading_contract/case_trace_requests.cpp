/**
 * tests/shading_contract/case_trace_requests.cpp
 *
 * ShadingContract_TraceRequests{Interp,Jit} (spec 026, T055; US6 AS1, FR-010, FR-011).
 *
 * With a CRecordingTracer installed in the services double, the trace builtins hand their rays to the
 * tracer instead of calling the hider:
 *   - visibility() (cm_visibility.sl) and trace()'s float form (the reflection probe, in cm_trace.sl)
 *     issue one probe request per shading point (four for the four-point state) through
 *     ITracer::traceRay, each with the ray type's flag (ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE for the
 *     transmission probe, ATTRIBUTES_FLAGS_SPECULAR_VISIBLE for the reflection probe), a unit direction,
 *     a positive maximum distance, time 0 and a bias, and consume the canned single-ray answer
 *     (hitT reaches the shader through the trace() float form);
 *   - trace()'s colour form and transmission() issue their *bundle* requests through
 *     ITracer::traceBundle: one bundle of four rays (the state's points) with the label the call
 *     used, bundle depth 0, the same flags. The recording tracer never runs the bundle protocol, so
 *     the load stub's traps stay unreached (constructing a bundle on the stack only writes a vtable
 *     pointer).
 * The two backends issue the same requests (the call sites are shared), so the Interp and Jit tests run
 * the same expectations. The bitwise equality of ordering and sample identity with the starting tree is
 * the replay's (ShadingContract_TraceReplay_*); here the sample identity is the legacy one (no frame
 * sampler is configured), so only structure is asserted.
 *
 * The standalone program has no object: the trace builtins read the object's attributes through
 * objectAttributes() (the parameter-list defaults, the interior and exterior shaders, the motion flag),
 * which is null then, and take the built-in defaults (FR-027).
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "recording_tracer.h"
#include "ri/geometry/object.h" // ATTRIBUTES_FLAGS_*

using namespace openrender::shading;

namespace {

    bool finiteUnit(const float *d) {
        const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        return std::isfinite(l) && std::fabs(l - 1.0f) < 1e-3f;
    }

    void checkRay(const STraceRay &r, int flags) {
        CT_EXPECT(r.flags == flags);
        CT_EXPECT(finiteUnit(r.dir) || (r.dir[0] * r.dir[0] + r.dir[1] * r.dir[1] + r.dir[2] * r.dir[2]) > 0.0f);
        CT_EXPECT(std::isfinite(r.from[0]) && std::isfinite(r.from[1]) && std::isfinite(r.from[2]));
        CT_EXPECT(r.t > 0.0f);
        CT_EXPECT(r.tmin >= 0.0f);
        CT_EXPECT(r.time == 0.0f);
        CT_EXPECT(std::isfinite(r.da) && std::isfinite(r.db));
    }

} // namespace

int caseTraceRequests(bool jit) {
    CContractServices svc;
    CRecordingTracer tracer;
    svc.tracerValue = &tracer;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);

        // ---- visibility(): the transmission probe form --------------------------------------------
        {
            printf("  cm_visibility (%s)\n", jit ? "slo" : "rslo");
            tracer.requests.clear();
            tracer.hit = false;
            CProgrammableShaderInstance *s = h.load("cm_visibility", jit);
            CT_EXPECT(s != nullptr);
            if (s != nullptr) {
                const SHarnessResult r = h.runSurface(s);
                CT_EXPECT(tracer.numRayRequests() == 4);
                CT_EXPECT(tracer.numBundleRequests() == 0);
                for (const STraceRequest &q : tracer.requests)
                    if (!q.bundle && q.rays.size() == 1)
                        checkRay(q.rays[0], ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE);
                for (int i = 0; i < h.numPoints; i++) {
                    const float v = r.ciAt(i, 0);
                    CT_EXPECT(v == 0.0f || v == 1.0f); // visibility() answers 0 or 1
                }
                CT_EXPECT(svc.tracerCalls > 0); // the tracer was asked of the services
            }
        }

        // ---- trace(): a bundle (the colour form) and a probe per point (the float form) -----------
        for (int pass = 0; pass < 2; pass++) {
            const bool canned = (pass == 1);
            printf("  cm_trace (%s)%s\n", jit ? "slo" : "rslo", canned ? ", canned hit" : "");
            tracer.requests.clear();
            tracer.hit = canned;
            tracer.hitT = 0.5f;
            CProgrammableShaderInstance *s = h.load("cm_trace", jit);
            CT_EXPECT(s != nullptr);
            if (s == nullptr)
                continue;
            const SHarnessResult r = h.runSurface(s);

            CT_EXPECT(tracer.numBundleRequests() == 1);
            CT_EXPECT(tracer.numRayRequests() == 4);
            for (const STraceRequest &q : tracer.requests) {
                if (q.bundle) {
                    CT_EXPECT((int)q.rays.size() == h.numPoints);
                    CT_EXPECT(q.bundleDepth == 0);
                    CT_EXPECT(q.label != "-");
                    for (const STraceRay &ray : q.rays)
                        checkRay(ray, ATTRIBUTES_FLAGS_SPECULAR_VISIBLE);
                }
                else {
                    CT_EXPECT(q.rays.size() == 1);
                    checkRay(q.rays[0], ATTRIBUTES_FLAGS_SPECULAR_VISIBLE);
                }
            }
            // The canned single-ray answer reaches the shader: the float form returns the hit distance
            // (Ci = c + color(d * 0.001), and the bundle was never run, so c = 0)
            if (canned)
                for (int i = 0; i < h.numPoints; i++)
                    CT_EXPECT_NEAR(r.ciAt(i, 0), 0.5f * 0.001f);
        }

        // ---- transmission(): a bundle --------------------------------------------------------------
        {
            printf("  cm_transmission (%s)\n", jit ? "slo" : "rslo");
            tracer.requests.clear();
            tracer.hit = false;
            CProgrammableShaderInstance *s = h.load("cm_transmission", jit);
            CT_EXPECT(s != nullptr);
            if (s != nullptr) {
                h.runSurface(s);
                CT_EXPECT(tracer.numBundleRequests() == 1);
                CT_EXPECT(tracer.numRayRequests() == 0);
                for (const STraceRequest &q : tracer.requests) {
                    CT_EXPECT((int)q.rays.size() == h.numPoints);
                    CT_EXPECT(q.bundleDepth == 0);
                    for (const STraceRay &ray : q.rays)
                        checkRay(ray, ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE);
                }
            }
        }

        // The tracer is the only thing the trace builtins asked the services for
        CT_EXPECT(svc.count("getMessage") == 0);
        CT_EXPECT(svc.count("getMatrix") == 0);
    }
    // A tracer is installed: no "no tracer" warning
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
