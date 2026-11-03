/**
 * tests/shading_contract/case_point_source.cpp
 *
 * ShadingContract_PointSource{Interp,Jit} (spec 029, T038; T-G2; FR-018, FR-020, FR-021, US7 AS3). Standalone.
 *
 * shade() takes the description of a batch (openrender::shading::ShaderGlobals, per-batch fields only on input) and an
 * IPointSource that stands for the primitive (pointSource.h). A recording source is shaded in the three shapes the
 * renderer has, a constant surface (cm_standalone.sl) bound in the attributes so the executor runs under each backend, and:
 *   - a 2-D span with derivatives (a ray hit): the source is asked sample(0, n), sample(n, 2n), interpolate(3n), in that
 *     order, which are the arguments and the order of today's code (shading.cpp, the SHADING_2D derivative branch), and
 *     the derivative tail is built: u/v/du/dv of the two offset points follow the real point (u + du, v), (u, v + dv);
 *   - a grid (SHADING_2D_GRID): sample(0, uv), interpolate(uv);
 *   - a 0-D point set (BatchShape::PointSet): sample(0, n), interpolate(n), no derivative branch.
 * Every real point's Ci is the surface's (0.25 0.5 0.75).
 *
 * The source is a recording double: the case needs no CSurface (Geometry) and no renderer.
 *
 * Red on the starting tree: a build failure (there is no pointSource.h and no shade(ShaderGlobals, IPointSource)).
 */
#include "cases_029.h"

#include "case_common.h"
#include "common/memory.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "libshader/shading/pointSource.h"
#include "libshader/shading/shaderGlobals.h"
#include "libshader/shading/shaderRefs.h"

using namespace openrender::shading;

namespace {

    // The recording source: what shade() asks of the primitive, in order
    class CRecordingSource final : public IPointSource {
        public:
            struct SCall {
                    char kind; // 's' sample(start, count), 'i' interpolate(count)
                    int a, b;  // sample: start, count; interpolate: count, 0
            };
            mutable std::vector<SCall> calls;

            CRecordingSource(const CAttributes *a, unsigned int parameters) : attrs(a), requested(parameters) {}

            const CAttributes *attributes() const override { return attrs; }
            uint64_t stableId() const override { return 42; }
            const CSurface *surface() const override { return nullptr; }
            unsigned int requestedParameters() const override { return requested; }

            void sample(int start, int count, float **varying, float ***, unsigned int &) const override {
                calls.push_back({'s', start, count});
                // A flat patch facing the camera: dPdu = x, dPdv = y, N = z, P from (u, v)
                for (int i = start; i < start + count; i++) {
                    const float u = varying[VARIABLE_U][i];
                    const float v = varying[VARIABLE_V][i];
                    initv(varying[VARIABLE_P] + 3 * i, u, v, 1);
                    initv(varying[VARIABLE_PS] + 3 * i, u, v, 1);
                    initv(varying[VARIABLE_DPDU] + 3 * i, 1, 0, 0);
                    initv(varying[VARIABLE_DPDV] + 3 * i, 0, 1, 0);
                    initv(varying[VARIABLE_NG] + 3 * i, 0, 0, 1);
                    initv(varying[VARIABLE_N] + 3 * i, 0, 0, 1);
                }
            }
            void interpolate(int count, float **, float ***) const override { calls.push_back({'i', count, 0}); }

        private:
            const CAttributes *attrs;
            unsigned int requested;
    };

    bool callsAre(const CRecordingSource &s, std::initializer_list<CRecordingSource::SCall> want) {
        if (s.calls.size() != want.size())
            return false;
        size_t i = 0;
        for (const CRecordingSource::SCall &w : want) {
            const CRecordingSource::SCall &g = s.calls[i++];
            if (g.kind != w.kind || g.a != w.a || g.b != w.b)
                return false;
        }
        return true;
    }

    void printCalls(const CRecordingSource &s) {
        fprintf(stderr, "    calls:");
        for (const CRecordingSource::SCall &c : s.calls) {
            if (c.kind == 's')
                fprintf(stderr, " sample(%d, %d)", c.a, c.b);
            else
                fprintf(stderr, " interpolate(%d)", c.a);
        }
        fprintf(stderr, "\n");
    }

    // The inputs shade() expects the caller to have set: u, v, time and the incident vector (the ray hit also its du)
    void presetState(CShadingState *ss, int numReal, bool footprint) {
        for (int i = 0; i < numReal; i++) {
            ss->varying[VARIABLE_U][i] = 0.1f * (i + 1);
            ss->varying[VARIABLE_V][i] = 0.05f * (i + 1);
            ss->varying[VARIABLE_TIME][i] = 0;
            initv(ss->varying[VARIABLE_I] + 3 * i, 0, 0, 1);
            ss->varying[VARIABLE_DU][i] = footprint ? 0.01f : 0.0f;
            ss->varying[VARIABLE_DV][i] = footprint ? 0.02f : 0.0f;
        }
    }

    void expectSurfaceColour(CShadingState *ss, int numReal) {
        for (int i = 0; i < numReal; i++) {
            CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 0], 0.25f);
            CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 1], 0.5f);
            CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 2], 0.75f);
        }
    }

} // namespace

int casePointSource(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        CShadingContext &ctx = *h.context;
        CShadingState *ss = ctx.currentShadingState;

        // The surface the batches run, bound in the attributes the source reports
        CProgrammableShaderInstance *surface = h.load("cm_standalone", jit);
        CT_EXPECT(surface != nullptr);
        if (surface == nullptr)
            return contract_test::finish();
        h.attributes->bindSurface(ShaderGroupRef::retain(surface));

        // ---- a 2-D span with derivatives ----------------------------------------------------------------------------
        {
            printf("  a 2-D span with derivatives (%s)\n", jit ? "slo" : "rslo");
            const int n = 4;
            presetState(ss, n, true);
            CRecordingSource source(h.attributes, PARAMETER_DERIVATIVE | PARAMETER_P | PARAMETER_N);
            ShaderGlobals g;
            g.numPoints = n;
            g.shape = BatchShape::Span;
            g.rayType = RayType::Camera;
            g.hitMode = HitMode::Shade;
            g.pass = ShadingPass::Full;
            g.pathDepth = 0;
            ctx.shade(g, source);
            if (!callsAre(source, {{'s', 0, n}, {'s', n, 2 * n}, {'i', 3 * n, 0}}))
                printCalls(source);
            CT_EXPECT(callsAre(source, {{'s', 0, n}, {'s', n, 2 * n}, {'i', 3 * n, 0}}));

            // The tail: the du-offset copy (u + du, v) and the dv-offset copy (u, v + dv) follow each real point
            float **v = ss->varying;
            for (int i = 0; i < n; i++) {
                const int ju = n + 2 * i, jv = n + 2 * i + 1;
                CT_EXPECT_NEAR(v[VARIABLE_U][ju], v[VARIABLE_U][i] + v[VARIABLE_DU][i]);
                CT_EXPECT_NEAR(v[VARIABLE_V][ju], v[VARIABLE_V][i]);
                CT_EXPECT_NEAR(v[VARIABLE_U][jv], v[VARIABLE_U][i]);
                CT_EXPECT_NEAR(v[VARIABLE_V][jv], v[VARIABLE_V][i] + v[VARIABLE_DV][i]);
                CT_EXPECT_NEAR(v[VARIABLE_DU][ju], v[VARIABLE_DU][i]);
                CT_EXPECT_NEAR(v[VARIABLE_DV][ju], v[VARIABLE_DV][i]);
                CT_EXPECT_NEAR(v[VARIABLE_DU][jv], v[VARIABLE_DU][i]);
                CT_EXPECT_NEAR(v[VARIABLE_DV][jv], v[VARIABLE_DV][i]);
            }
            expectSurfaceColour(ss, n);
        }

        // ---- a grid --------------------------------------------------------------------------------------------------
        {
            printf("  a grid (%s)\n", jit ? "slo" : "rslo");
            const int gu = 3, gv = 2, n = gu * gv;
            presetState(ss, n, false);
            CRecordingSource source(h.attributes, PARAMETER_DERIVATIVE | PARAMETER_P | PARAMETER_N);
            ShaderGlobals g;
            g.numPoints = n;
            g.shape = BatchShape::Grid;
            g.gridU = gu;
            g.gridV = gv;
            g.rayType = RayType::None;
            g.hitMode = HitMode::Shade;
            g.pass = ShadingPass::Full;
            ctx.shade(g, source);
            if (!callsAre(source, {{'s', 0, n}, {'i', n, 0}}))
                printCalls(source);
            CT_EXPECT(callsAre(source, {{'s', 0, n}, {'i', n, 0}}));
            expectSurfaceColour(ss, n);
        }

        // ---- a 0-D point set -------------------------------------------------------------------------------------------
        {
            printf("  a 0-D point set (%s)\n", jit ? "slo" : "rslo");
            const int n = 4;
            presetState(ss, n, false);
            CRecordingSource source(h.attributes, PARAMETER_DERIVATIVE | PARAMETER_P | PARAMETER_N);
            ShaderGlobals g;
            g.numPoints = n;
            g.shape = BatchShape::PointSet;
            g.rayType = RayType::None;
            g.hitMode = HitMode::Shade;
            g.pass = ShadingPass::Full;
            ctx.shade(g, source);
            if (!callsAre(source, {{'s', 0, n}, {'i', n, 0}}))
                printCalls(source);
            CT_EXPECT(callsAre(source, {{'s', 0, n}, {'i', n, 0}}));
            expectSurfaceColour(ss, n);
        }
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
