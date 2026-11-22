/**
 * Project: openRender
 *
 * File: case_area_light.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), Phase 4, tasks.md T063: the bound-light branch of CShadingContext::iterateLights, driven
 *   through a double IAreaLightSampler (known emitter points, known densities, a log of the draws it was given) and a double
 *   tracer whose occluded() answers from a script, on a rendered session (real shaders, the interpreter and the JIT). It
 *   asserts the nine points of the task:
 *     (1) the light shader sees the double's P, N, Ng, dPdu, dPdv, u, v, s, t, and the receiver's Ps (Cl encodes them);
 *     (2) the receiver's P, N and every varying array are byte-identical before and after the lights run (a light that
 *         writes N is the stress), and the lightSideN snapshot still holds the receiver's N;
 *     (3) each pushed CShadedLight has Cl scaled by exactly V / (n * pdfA) for n in {1, 3, 16}, L unscaled, one entry per
 *         sample (FR-012), and the shadow probe is asked with TRANSMISSION visibility, the light's identity to exclude and
 *         a target and bias that reach the emitter point;
 *     (4) a vertex with pdfA == 0 pushes Cl == 0;
 *     (5) the receiver is skipped when its attributes' areaLight is the light;
 *     (6) the shading dimension is restored after the run;
 *     (7) a classic light in the same loop is untouched, the entries keep the lights' order, and __category acts on a
 *         bound light's entries as on a classic one;
 *     (8) the draws are a pure function of (point, light, sample): the same keys give the same (u1, u2) in a second run,
 *         other keys give others, and the sampler gets every real vertex at once;
 *     (9) with no sampler installed every light runs as a classic light: one entry, Cl unscaled, the receiver's P;
 *    (10) a batch with passive points (tags != 0; GitHub #100: iterateLights publishes the receiver's counts for the run of
 *         the lights and puts the state's own back): the passive vertex gets no probe and keeps what a classic light leaves
 *         there (not weighted), the active vertices get the value of (3), the state's numActive and numPassive read afterwards
 *         what they read before, and a batch with every point passive pushes no entry (as a classic light), asks no probe
 *         and returns;
 *    (11) __nondiffuse and __nonspecular on a bound light act as on a classic light: a light at `from` (whose __nondiffuse or __nonspecular is an
 *         output variable, read by the engine from the entry's saved state) gives the receiver's diffuse() and specular() the same result, bit for bit, as
 *         the same light run as a classic one (the bound light's one sample has density 1 and the shader uses `from`, so the entries agree), in both formats;
 *         and, for .rslo, 0 is the full result, 0.5 half of it, 1 none, for a varying and a uniform output variable (the scaling under the JIT is pending
 *         an engine issue: see the comment at the case).
 *
 *   Written against master's services 2.3.0 plus this spec's interface (areaLightSampler.h, ITracer::occluded; services 2.6.0
 *   with it): red on the starting tree (does not compile). Spec 033's 2.4.0 request members change what a services double must
 *   answer; this double uses the real services of the session (s.original) and is adapted in the 033 sync if needed.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "cases_057.h"

#include <cmath>
#include <vector>

#include "case_common.h"
#include "common/sampling.h"
#include "libshader/shading/areaLightSampler.h"
#include "libshader/shading/tracer.h"
#include "rendered_harness.h"
#include "ri/core/ray.h"

using namespace openrender::shading;

namespace {

    constexpr int kVertices = 3;
    // the density the double reports for vertex i (vertex 2 has no point in the "zero" scenarios: pdfA = 0)
    constexpr float kPdf[kVertices] = {0.5f, 0.25f, 0.125f};
    constexpr float kPdfWithZero[kVertices] = {0.5f, 0.25f, 0.0f};
    constexpr float kPdfOne[kVertices] = {1.0f, 1.0f, 1.0f};

    // The emitter point sample k of vertex i: every global has its own recognisable function of (i, k)
    struct SPoint {
            float P[3], N[3], Ng[3], dPdu[3], dPdv[3], u, v, s, t;
    };
    SPoint pointOf(int i, int k) {
        SPoint p;
        const float a[3] = {10.0f + i, 20.0f + k, 30.0f};
        const float n[3] = {0.0f, 0.0f, -1.0f};
        const float ng[3] = {0.1f * (i + 1), 0.2f + 0.01f * k, 0.3f};
        const float du[3] = {1.0f + i, 0.0f, 0.5f};
        const float dv[3] = {0.0f, 2.0f + k, 0.25f};
        for (int c = 0; c < 3; c++) {
            p.P[c] = a[c];
            p.N[c] = n[c];
            p.Ng[c] = ng[c];
            p.dPdu[c] = du[c];
            p.dPdv[c] = dv[c];
        }
        p.u = 0.1f * (i + 1);
        p.v = 0.01f * (k + 1);
        p.s = 0.5f + 0.1f * i;
        p.t = 0.25f + 0.01f * k;
        return p;
    }

    // The double sampler: `light` is the one bound light; every call of sampleEmitters is one sample k, logged
    class CAreaDouble : public IAreaLightSampler {
        public:
            const CShaderGroupRefTarget *bound = nullptr;
            AreaLightInfo info;
            const float *pdf = kPdf;
            int areaLightQueries = 0;
            int sampleCalls = 0;
            std::vector<int> pointsPerCall;
            std::vector<std::vector<float>> u1Log, u2Log, timeLog; // per call, per vertex

            AreaLightInfo areaLight(CShadingContext &, const CShaderGroupRefTarget *light) override {
                ++areaLightQueries;
                return light == bound ? info : AreaLightInfo();
            }

            void sampleEmitters(CShadingContext &, const CShaderGroupRefTarget *light, int m, const float *u1, const float *u2, const float *time, float **out,
                                float *pdfA) override {
                CT_EXPECT(light == bound);
                const int k = sampleCalls++;
                pointsPerCall.push_back(m);
                u1Log.emplace_back(u1, u1 + m);
                u2Log.emplace_back(u2, u2 + m);
                timeLog.emplace_back(time, time + m);
                for (int i = 0; i < m; i++) {
                    const int v = i < kVertices ? i : i % kVertices; // a derivative tail vertex copies a real vertex's point
                    const SPoint p = pointOf(v, k);
                    for (int c = 0; c < 3; c++) {
                        out[VARIABLE_P][3 * i + c] = p.P[c];
                        out[VARIABLE_N][3 * i + c] = p.N[c];
                        out[VARIABLE_NG][3 * i + c] = p.Ng[c];
                        out[VARIABLE_DPDU][3 * i + c] = p.dPdu[c];
                        out[VARIABLE_DPDV][3 * i + c] = p.dPdv[c];
                    }
                    out[VARIABLE_U][i] = p.u;
                    out[VARIABLE_V][i] = p.v;
                    out[VARIABLE_S][i] = p.s;
                    out[VARIABLE_T][i] = p.t;
                    pdfA[i] = pdf[v];
                }
            }
    };

    // The double tracer: occluded() answers from a script keyed by the emitter point the probe reaches (k from its y, i from
    // its x), and records what it was asked
    class CShadowDouble : public ITracer {
        public:
            const CShaderGroupRefTarget *expectedExclude = nullptr;
            bool (*blocked)(int vertex, int sample) = nullptr;
            int asked = 0, badFlags = 0, badExclude = 0, badTarget = 0;
            int askedAt[kVertices] = {0, 0, 0}; // per receiver vertex (the probe's target says which)

            void traceRay(CShadingContext &, CRay *ray) override { ray->object = nullptr; }
            void traceBundle(CShadingContext &, CRayBundle *) override {}
            bool occluded(CShadingContext &, CRay *ray, const CShaderGroupRefTarget *excludeLight) override {
                ++asked;
                if (!(ray->flags & ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE))
                    ++badFlags;
                if (excludeLight != expectedExclude)
                    ++badExclude;
                // the probe reaches the emitter point: from + dir * (t + tmin) (t is the distance minus the bias, tmin the bias)
                const float dist = ray->t + ray->tmin;
                float target[3];
                for (int c = 0; c < 3; c++)
                    target[c] = ray->from[c] + ray->dir[c] * dist;
                const int i = static_cast<int>(std::lround(target[0] - 10.0f));
                const int k = static_cast<int>(std::lround(target[1] - 20.0f));
                if (i < 0 || i >= kVertices || k < 0 || std::fabs(target[2] - 30.0f) > 1e-3f || std::fabs(target[0] - 10.0f - i) > 1e-3f)
                    ++badTarget;
                if (i >= 0 && i < kVertices)
                    ++askedAt[i];
                return blocked != nullptr && blocked(i, k);
            }
    };

    bool never(int, int) { return false; }
    bool someBlocked(int i, int k) { return ((k + i) % 3) == 2; }

    // One run of a surface over three points with the lights of `attributes`
    struct SRun {
            std::vector<float> ci;                          // 3 floats per point
            std::vector<const CShaderInstance *> entryOf;   // the light of each pushed entry, list order
            std::vector<std::vector<float>> entryCl, entryL; // per entry, 3 floats per point
            bool varyingsUnchanged = true;
            bool lightSideNHolds = true;
            EShadingDim dimBefore = SHADING_0D, dimAfter = SHADING_0D;
            int numActiveBefore = 0, numPassiveBefore = 0, numActiveAfter = 0, numPassiveAfter = 0;
    };

    const int kSnapshot[] = {VARIABLE_P, VARIABLE_N, VARIABLE_NG, VARIABLE_DPDU, VARIABLE_DPDV, VARIABLE_U, VARIABLE_V, VARIABLE_S, VARIABLE_T, VARIABLE_PS};

    // `tags` (default: every point active) makes the points with a non-zero entry passive, the counts set to match
    SRun runSurface(CRenderedSession &s, CRProbeContext *ctx, CProgrammableShaderInstance *surface, CAttributes *attributes, IAreaLightSampler *sampler, int keySeed = 0,
                    const int *tags = nullptr) {
        CRProbeSurface *object = new CRProbeSurface(attributes, s.xform);
        CRenderedSession::setupState(ctx, kVertices, object);
        CShadingState *ss = ctx->currentShadingState;
        ctx->setAreaLightSampler(sampler);
        if (ss->pointKeys != nullptr)
            for (int i = 0; i < kVertices; i++)
                ss->pointKeys[i] = openrender::sampling::cellKey(100 + i + keySeed, 0.5f, 0.25f, 0, 1, 0);
        ss->currentShaderInstance = surface;
        if (tags != nullptr) {
            ss->numActive = 0;
            for (int i = 0; i < kVertices; i++) {
                ss->tags[i] = tags[i];
                ss->numActive += (tags[i] == 0);
            }
            ss->numPassive = kVertices - ss->numActive;
        }

        std::vector<std::vector<float>> before;
        for (int var : kSnapshot)
            before.emplace_back(ss->varying[var], ss->varying[var] + 3 * kVertices);
        std::vector<float> sideBefore(ss->varying[VARIABLE_N], ss->varying[VARIABLE_N] + 3 * kVertices);

        SRun r;
        r.dimBefore = ss->shadingDim;
        r.numActiveBefore = ss->numActive;
        r.numPassiveBefore = ss->numPassive;
        CMemPage *mem = nullptr;
        memoryInit(mem);
        float **locals = surface->prepare(mem, ss->varying, ss->numVertices);
        ctx->execute(surface, locals);
        r.dimAfter = ss->shadingDim;
        r.numActiveAfter = ss->numActive;
        r.numPassiveAfter = ss->numPassive;

        for (int i = 0; i < kVertices; i++)
            r.ci.insert(r.ci.end(), ss->varying[VARIABLE_CI] + 3 * i, ss->varying[VARIABLE_CI] + 3 * i + 3);
        for (CShadedLight *l = ss->lights; l != nullptr; l = l->next) {
            r.entryOf.push_back(l->instance);
            r.entryL.emplace_back(l->savedState[0], l->savedState[0] + 3 * kVertices);
            r.entryCl.emplace_back(l->savedState[1], l->savedState[1] + 3 * kVertices);
        }
        size_t at = 0;
        for (int var : kSnapshot) {
            if (std::memcmp(before[at].data(), ss->varying[var], sizeof(float) * 3 * kVertices) != 0)
                r.varyingsUnchanged = false;
            ++at;
        }
        if (ss->lightSideNStore != nullptr && std::memcmp(sideBefore.data(), ss->lightSideNStore, sizeof(float) * 3 * kVertices) != 0)
            r.lightSideNHolds = false;
        ss->lights = nullptr;
        ss->freeLights = nullptr;
        ss->currentShaderInstance = nullptr;
        ctx->setAreaLightSampler(nullptr);
        memoryTini(mem);
        return r;
    }

    // Case (10): iterateLights called directly with the counts of `tags` (the receiver's, as its illuminance would publish them) while
    // the state's own counts stay what setupState left (every point active), so a published count that is not put back shows
    struct SDirectRun {
            std::vector<std::vector<float>> entryCl; // per pushed entry, 3 floats per point
            int numActiveBefore = 0, numPassiveBefore = 0, numActiveAfter = 0, numPassiveAfter = 0;
            int calledActive = 0, calledPassive = 0; // the counts iterateLights was given, as it left them
            bool varyingsUnchanged = true;
    };
    SDirectRun runDirect(CRenderedSession &s, CRProbeContext *ctx, CProgrammableShaderInstance *surface, CAttributes *attributes, IAreaLightSampler *sampler, const int *tags) {
        CRProbeSurface *object = new CRProbeSurface(attributes, s.xform);
        CRenderedSession::setupState(ctx, kVertices, object);
        CShadingState *ss = ctx->currentShadingState;
        ctx->setAreaLightSampler(sampler);
        if (ss->pointKeys != nullptr)
            for (int i = 0; i < kVertices; i++)
                ss->pointKeys[i] = openrender::sampling::cellKey(100 + i, 0.5f, 0.25f, 0, 1, 0);
        ss->currentShaderInstance = surface;
        int numActive = 0;
        for (int i = 0; i < kVertices; i++) {
            ss->tags[i] = tags[i];
            numActive += (tags[i] == 0);
        }
        int numPassive = kVertices - numActive;
        float lP[3 * kVertices], lN[3 * kVertices], lT[kVertices];
        for (int i = 0; i < kVertices; i++) {
            memcpy(lP + 3 * i, ss->varying[VARIABLE_PS] + 3 * i, 3 * sizeof(float));
            const float nn[3] = {0, 0, 1};
            memcpy(lN + 3 * i, nn, sizeof(nn));
            lT[i] = 0.0f;
        }
        std::vector<std::vector<float>> before;
        for (int var : kSnapshot)
            before.emplace_back(ss->varying[var], ss->varying[var] + 3 * kVertices);

        SDirectRun r;
        r.numActiveBefore = ss->numActive;
        r.numPassiveBefore = ss->numPassive;
        ctx->iterateLights(lP, lN, lT, kVertices, ss->tags, numActive, numPassive, FALSE, ss->varying, surface);
        r.numActiveAfter = ss->numActive;
        r.numPassiveAfter = ss->numPassive;
        r.calledActive = numActive;
        r.calledPassive = numPassive;
        for (CShadedLight *l = ss->lights; l != nullptr; l = l->next)
            r.entryCl.emplace_back(l->savedState[1], l->savedState[1] + 3 * kVertices);
        size_t at = 0;
        for (int var : kSnapshot) {
            if (std::memcmp(before[at].data(), ss->varying[var], sizeof(float) * 3 * kVertices) != 0)
                r.varyingsUnchanged = false;
            ++at;
        }
        ss->lights = nullptr;
        ss->freeLights = nullptr;
        ss->currentShaderInstance = nullptr;
        ctx->setAreaLightSampler(nullptr);
        return r;
    }

    // A light instance with one float or one colour parameter
    CProgrammableShaderInstance *loadLight(const char *name, float which) {
        const char *tokens[] = {"float which"};
        const void *params[] = {&which};
        CShaderInstance *inst = CRenderer::context->getShader(name, SL_LIGHTSOURCE, 1, tokens, params);
        return dynamic_cast<CProgrammableShaderInstance *>(inst);
    }
    // A light instance with one named float parameter, or none (token null)
    CProgrammableShaderInstance *loadLightParam(const char *name, const char *token, float value) {
        const char *tokens[] = {token};
        const void *params[] = {&value};
        CShaderInstance *inst = CRenderer::context->getShader(name, SL_LIGHTSOURCE, token != nullptr ? 1 : 0, tokens, params);
        return dynamic_cast<CProgrammableShaderInstance *>(inst);
    }
    CProgrammableShaderInstance *loadConst(CRenderedSession &s, const char *name, float r, float g, float b) {
        const float c[3] = {r, g, b};
        const char *tokens[] = {"color c"};
        const void *params[] = {c};
        (void)s;
        CShaderInstance *inst = CRenderer::context->getShader(name, SL_LIGHTSOURCE, 1, tokens, params);
        return dynamic_cast<CProgrammableShaderInstance *>(inst);
    }
    const CShaderGroupRefTarget *identityOf(CProgrammableShaderInstance *light) { return light; }

    // GitHub #126: a .slo surface whose only lighting call is illuminance(P) gathers no light, so the receivers of this case load as
    // .rslo in the Jit run too (the lights stay in the run's format). The session's Option is out of scope inside the world block
    // ("Bad scope for RiOption"), so the format is chosen with the "shade" attribute, which wins over the option. Remove when #126 is fixed.
    CProgrammableShaderInstance *loadReceiver(CRenderedSession &s, const char *name, bool jit) {
        RtString rslo = const_cast<RtString>("rslo"), slo = const_cast<RtString>("slo");
        if (jit)
            RiAttribute((RtToken) "shade", (RtToken) "shaderformat", &rslo, RI_NULL);
        CProgrammableShaderInstance *surface = s.load(name, SL_SURFACE);
        if (jit)
            RiAttribute((RtToken) "shade", (RtToken) "shaderformat", &slo, RI_NULL);
        return surface;
    }

    // The entries of one light, in list order
    std::vector<int> entriesOf(const SRun &r, const CShaderInstance *light) {
        std::vector<int> e;
        for (size_t k = 0; k < r.entryOf.size(); k++)
            if (r.entryOf[k] == light)
                e.push_back(static_cast<int>(k));
        return e;
    }

} // namespace

int caseAreaLight(bool jit) {
    CRenderedSession s(jit);
    CRProbeContext *ctx = s.makeContext(s.original);
    CShadowDouble tracer;
    ctx->setTracer(&tracer);
    printf("AreaLight contract (%s)\n", jit ? "slo" : "rslo");

    CProgrammableShaderInstance *sum = loadReceiver(s, "ca_surface_sum", jit);
    CT_EXPECT(sum != nullptr);
    if (sum == nullptr)
        return contract_test::finish();

    // ---- (1) the light shader sees the double's globals and the receiver's Ps ----------------------------------------------
    for (int which = 0; which <= 7; which++) {
        CProgrammableShaderInstance *light = loadLight("ca_light_encode", static_cast<float>(which));
        CT_EXPECT(light != nullptr);
        if (light == nullptr)
            continue;
        CAttributes *attributes = new CAttributes();
        attributes->addLight(light);
        CAreaDouble sampler;
        sampler.bound = identityOf(light);
        sampler.info.bound = true;
        sampler.info.sceneSamples = 1;
        sampler.info.sceneMode = AreaLightMode::Full;
        sampler.info.totalArea = 4.0;
        tracer.expectedExclude = identityOf(light);
        tracer.blocked = never;
        const SRun r = runSurface(s, ctx, sum, attributes, &sampler);
        CT_EXPECT(sampler.sampleCalls == 1);
        for (int i = 0; i < kVertices; i++) {
            const SPoint p = pointOf(i, 0);
            float want[3];
            switch (which) {
                case 0: memcpy(want, p.P, sizeof want); break;
                case 1: memcpy(want, p.N, sizeof want); break;
                case 2: memcpy(want, p.Ng, sizeof want); break;
                case 3: memcpy(want, p.dPdu, sizeof want); break;
                case 4: memcpy(want, p.dPdv, sizeof want); break;
                case 5: want[0] = p.u; want[1] = p.v; want[2] = 0; break;
                case 6: want[0] = p.s; want[1] = p.t; want[2] = 0; break;
                default: { // the receiver's Ps from the harness state
                    want[0] = 0.1f * (i + 1);
                    want[1] = 0.2f * (i + 1);
                    want[2] = 0.3f * (i + 1);
                } break;
            }
            for (int c = 0; c < 3; c++)
                CT_EXPECT_NEAR(r.ci[3 * i + c], want[c] / kPdf[i]); // weighted by exactly V / (n pdfA), n = 1, V = 1
        }
        CT_EXPECT(r.varyingsUnchanged);
    }

    // ---- (2) the receiver's globals are untouched by a light that writes N; the lightSideN snapshot holds -----------------------
    {
        CProgrammableShaderInstance *light = loadConst(s, "ca_light_writen", 1, 1, 1);
        CT_EXPECT(light != nullptr);
        CAttributes *attributes = new CAttributes();
        attributes->addLight(light);
        CAreaDouble sampler;
        sampler.bound = identityOf(light);
        sampler.info = AreaLightInfo();
        sampler.info.bound = true;
        sampler.info.sceneSamples = 2;
        sampler.info.sceneMode = AreaLightMode::Full;
        sampler.info.totalArea = 4.0;
        tracer.expectedExclude = identityOf(light);
        tracer.blocked = never;
        const SRun r = runSurface(s, ctx, sum, attributes, &sampler);
        CT_EXPECT(sampler.sampleCalls == 2);
        CT_EXPECT(r.varyingsUnchanged);
        CT_EXPECT(r.lightSideNHolds);
    }

    // ---- (3) the exact scale V / (n pdfA), L unscaled, one entry per sample; (4) pdfA == 0 gives 0; (8) the draws ----------------
    CProgrammableShaderInstance *constLight = loadConst(s, "ca_light_const", 1.0f, 2.0f, 3.0f);
    CT_EXPECT(constLight != nullptr);
    std::vector<std::vector<float>> firstLog;
    for (int n : {1, 3, 16}) {
        CAttributes *attributes = new CAttributes();
        attributes->addLight(constLight);
        CAreaDouble sampler;
        sampler.bound = identityOf(constLight);
        sampler.info.bound = true;
        sampler.info.sceneSamples = n;
        sampler.info.sceneMode = AreaLightMode::Full;
        sampler.info.totalArea = 4.0;
        sampler.pdf = kPdfWithZero;
        tracer.expectedExclude = identityOf(constLight);
        tracer.blocked = someBlocked;
        tracer.asked = tracer.badFlags = tracer.badExclude = tracer.badTarget = 0;
        const SRun r = runSurface(s, ctx, sum, attributes, &sampler);
        CT_EXPECT(sampler.sampleCalls == n);
        const std::vector<int> mine = entriesOf(r, constLight);
        CT_EXPECT(static_cast<int>(mine.size()) == n); // one CShadedLight per sample
        for (int i = 0; i < kVertices; i++) {
            float sum1 = 0;
            for (int k = 0; k < n; k++) {
                const float v = someBlocked(i, k) ? 0.0f : 1.0f;
                sum1 += kPdfWithZero[i] > 0.0f ? v / (n * kPdfWithZero[i]) : 0.0f;
            }
            for (int c = 0; c < 3; c++)
                CT_EXPECT_NEAR(r.ci[3 * i + c], sum1 * (1.0f + c)); // c = (1, 2, 3)
        }
        // per entry: Cl is 0 or exactly c / (n pdfA) at a vertex with a point; always 0 at the vertex with pdfA == 0
        for (int e : mine) {
            for (int i = 0; i < kVertices; i++) {
                const float cl0 = r.entryCl[e][3 * i];
                if (kPdfWithZero[i] == 0.0f)
                    CT_EXPECT(cl0 == 0.0f && r.entryCl[e][3 * i + 1] == 0.0f && r.entryCl[e][3 * i + 2] == 0.0f);
                else
                    CT_EXPECT(cl0 == 0.0f || std::fabs(cl0 - 1.0f / (n * kPdfWithZero[i])) <= 1e-6f * (1.0f + cl0));
            }
        }
        // L is the light's own, unscaled: Ps - P(emitter) for every sample, once per sample
        for (int i = 0; i < kVertices; i++)
            for (int k = 0; k < n; k++) {
                const SPoint p = pointOf(i, k);
                const float want[3] = {p.P[0] - 0.1f * (i + 1), p.P[1] - 0.2f * (i + 1), p.P[2] - 0.3f * (i + 1)}; // the saved L is the direction to the light, -(Ps - P) (ENDILLUMINATE)
                int found = 0;
                for (int e : mine)
                    if (std::fabs(r.entryL[e][3 * i] - want[0]) < 1e-4f && std::fabs(r.entryL[e][3 * i + 1] - want[1]) < 1e-4f && std::fabs(r.entryL[e][3 * i + 2] - want[2]) < 1e-4f)
                        ++found;
                CT_EXPECT(found == 1);
            }
        // the probes: one per vertex with a point and a non-zero Cl per sample, with the right flags, exclusion and target
        CT_EXPECT(tracer.badFlags == 0 && tracer.badExclude == 0 && tracer.badTarget == 0);
        CT_EXPECT(tracer.asked == 2 * n); // vertices 0 and 1 (vertex 2 has no point), every sample
        // (8) the draws: every real vertex at once, in [0, 1), a function of (point, light, sample)
        CT_EXPECT(sampler.pointsPerCall.size() == static_cast<size_t>(n));
        for (int k = 0; k < n; k++) {
            CT_EXPECT(sampler.pointsPerCall[k] >= kVertices);
            for (int i = 0; i < kVertices; i++)
                CT_EXPECT(sampler.u1Log[k][i] >= 0.0f && sampler.u1Log[k][i] < 1.0f && sampler.u2Log[k][i] >= 0.0f && sampler.u2Log[k][i] < 1.0f);
        }
        if (n == 3) {
            firstLog = sampler.u1Log;
            // the same keys, the same draws
            CAreaDouble again;
            again.bound = sampler.bound;
            again.info = sampler.info;
            again.pdf = kPdfWithZero;
            CAttributes *attributes2 = new CAttributes();
            attributes2->addLight(constLight);
            runSurface(s, ctx, sum, attributes2, &again);
            CT_EXPECT(again.u1Log == sampler.u1Log && again.u2Log == sampler.u2Log);
            // other point keys, other draws
            CAreaDouble other;
            other.bound = sampler.bound;
            other.info = sampler.info;
            other.pdf = kPdfWithZero;
            CAttributes *attributes3 = new CAttributes();
            attributes3->addLight(constLight);
            runSurface(s, ctx, sum, attributes3, &other, 1000);
            CT_EXPECT(other.u1Log != sampler.u1Log);
            // the samples of one vertex differ from each other
            CT_EXPECT(sampler.u1Log[0][0] != sampler.u1Log[1][0] || sampler.u2Log[0][0] != sampler.u2Log[1][0]);
        }
    }

    // ---- (5) the receiver is skipped when its attributes' areaLight is the light ------------------------------------------------
    {
        CAttributes *attributes = new CAttributes();
        attributes->addLight(constLight);
        attributes->areaLight = identityOf(constLight);
        CAreaDouble sampler;
        sampler.bound = identityOf(constLight);
        sampler.info.bound = true;
        sampler.info.sceneSamples = 4;
        sampler.info.sceneMode = AreaLightMode::Full;
        sampler.info.totalArea = 4.0;
        const SRun r = runSurface(s, ctx, sum, attributes, &sampler);
        CT_EXPECT(sampler.sampleCalls == 0);
        CT_EXPECT(entriesOf(r, constLight).empty());
        for (size_t k = 0; k < r.ci.size(); k++)
            CT_EXPECT(r.ci[k] == 0.0f);
    }

    // ---- (6) the shading dimension is restored; (7) a classic light, the order of the entries and __category -----------------------
    {
        CProgrammableShaderInstance *classic = loadConst(s, "ca_light_const", 1.0f, 0.0f, 0.0f);
        CProgrammableShaderInstance *bound = loadConst(s, "ca_light_const", 0.0f, 1.0f, 0.0f);
        CT_EXPECT(classic != nullptr && bound != nullptr);
        // the order the list takes for two classic lights: the pattern the bound light's entries must follow
        CAttributes *classicOnly = new CAttributes();
        classicOnly->addLight(classic);
        classicOnly->addLight(bound);
        const SRun base = runSurface(s, ctx, sum, classicOnly, nullptr);
        CT_EXPECT(base.entryOf.size() == 2);
        const bool boundFirst = base.entryOf.size() == 2 && base.entryOf[0] == bound;

        CAttributes *attributes = new CAttributes();
        attributes->addLight(classic);
        attributes->addLight(bound);
        CAreaDouble sampler;
        sampler.bound = identityOf(bound);
        sampler.info.bound = true;
        sampler.info.sceneSamples = 2;
        sampler.info.sceneMode = AreaLightMode::Full;
        sampler.info.totalArea = 4.0;
        tracer.expectedExclude = identityOf(bound);
        tracer.blocked = never;
        const SRun r = runSurface(s, ctx, sum, attributes, &sampler);
        CT_EXPECT(r.dimBefore == r.dimAfter);
        CT_EXPECT(r.entryOf.size() == 3);               // the classic light's one entry and the bound light's two
        CT_EXPECT(entriesOf(r, classic).size() == 1);
        CT_EXPECT(entriesOf(r, bound).size() == 2);
        // order: the bound light's entries are adjacent and on the same side of the classic one as in the classic-only run
        const std::vector<int> b = entriesOf(r, bound), c = entriesOf(r, classic);
        if (b.size() == 2 && c.size() == 1) {
            CT_EXPECT(b[1] == b[0] + 1);
            CT_EXPECT((boundFirst && b[0] < c[0]) || (!boundFirst && c[0] < b[0]));
        }
        // the classic light is untouched: Cl = (1, 0, 0) unscaled at every point; the bound one is scaled
        for (int i = 0; i < kVertices; i++) {
            CT_EXPECT_NEAR(r.ci[3 * i + 0], 1.0f);                       // only the classic light has red
            CT_EXPECT_NEAR(r.ci[3 * i + 1], 2.0f / (2 * kPdf[i]));        // two unblocked samples of 1 / (n pdfA)
        }
        for (int e : c)
            for (int i = 0; i < kVertices; i++)
                CT_EXPECT_NEAR(r.entryCl[e][3 * i], 1.0f);
    }
    {
        // __category: a bound light in a category is seen by illuminance(category) and not by another category
        CProgrammableShaderInstance *light = loadConst(s, "ca_light_cat", 0.0f, 0.0f, 1.0f);
        CProgrammableShaderInstance *byCategory = loadReceiver(s, "ca_surface_sum_cat", jit);
        CT_EXPECT(light != nullptr && byCategory != nullptr);
        if (light != nullptr && byCategory != nullptr) {
            CAttributes *attributes = new CAttributes();
            attributes->addLight(light);
            CAreaDouble sampler;
            sampler.bound = identityOf(light);
            sampler.info.bound = true;
            sampler.info.sceneSamples = 2;
            sampler.info.sceneMode = AreaLightMode::Full;
            sampler.info.totalArea = 4.0;
            tracer.expectedExclude = identityOf(light);
            tracer.blocked = never;
            const SRun r = runSurface(s, ctx, byCategory, attributes, &sampler);
            CT_EXPECT(sampler.sampleCalls == 2);
            for (int i = 0; i < kVertices; i++)
                CT_EXPECT_NEAR(r.ci[3 * i + 2], 2.0f / (2 * kPdf[i]));
        }
    }

    // ---- (9) with no sampler every light runs as a classic light: one entry, Cl unscaled, the receiver's P ---------------------------
    {
        CProgrammableShaderInstance *light = loadLight("ca_light_encode", 0.0f);
        CT_EXPECT(light != nullptr);
        CAttributes *attributes = new CAttributes();
        attributes->addLight(light);
        const SRun r = runSurface(s, ctx, sum, attributes, nullptr);
        CT_EXPECT(entriesOf(r, light).size() == 1);
        for (int i = 0; i < kVertices; i++) {
            CT_EXPECT_NEAR(r.ci[3 * i + 0], 0.1f * (i + 1));
            CT_EXPECT_NEAR(r.ci[3 * i + 1], 0.2f * (i + 1));
            CT_EXPECT_NEAR(r.ci[3 * i + 2], 0.3f * (i + 1));
        }
    }

    // ---- (10) passive points: the receiver's counts are published for the run of the lights and put back (GitHub #100) ---------------
    {
        const int mixed[kVertices] = {0, 1, 0}; // the middle point is passive
        const int none[kVertices] = {1, 1, 1};  // no point is active
        for (int n : {1, 3}) {
            CAttributes *attributes = new CAttributes();
            attributes->addLight(constLight);
            CAreaDouble sampler;
            sampler.bound = identityOf(constLight);
            sampler.info.bound = true;
            sampler.info.sceneSamples = n;
            sampler.info.sceneMode = AreaLightMode::Full;
            sampler.info.totalArea = 4.0;
            tracer.expectedExclude = identityOf(constLight);
            tracer.blocked = never;
            tracer.asked = tracer.badFlags = tracer.badExclude = tracer.badTarget = 0;
            for (int &a : tracer.askedAt)
                a = 0;
            // what a classic light leaves at the passive vertex (its Cl is not written there, and not zero): the bound light must leave it so
            CAttributes *classicAttributes = new CAttributes();
            classicAttributes->addLight(constLight);
            const SDirectRun classic = runDirect(s, ctx, sum, classicAttributes, nullptr, mixed);
            CT_EXPECT(classic.entryCl.size() == 1);
            const SDirectRun r = runDirect(s, ctx, sum, attributes, &sampler, mixed);
            CT_EXPECT(sampler.sampleCalls == n);
            CT_EXPECT(static_cast<int>(r.entryCl.size()) == n); // one entry per sample
            for (const std::vector<float> &cl : r.entryCl) {
                for (int c = 0; c < 3; c++) {
                    CT_EXPECT(!classic.entryCl.empty() && cl[3 * 1 + c] == classic.entryCl[0][3 * 1 + c]); // (a) the passive vertex is left as a classic light leaves it: not scaled
                    for (int i : {0, 2})
                        CT_EXPECT_NEAR(cl[3 * i + c], (1.0f + c) / (n * kPdf[i])); // (b) the active vertices get the value of case (3)
                }
            }
            CT_EXPECT(tracer.askedAt[1] == 0);             // (a) and no probe for it
            CT_EXPECT(tracer.askedAt[0] == n && tracer.askedAt[2] == n);
            CT_EXPECT(tracer.badFlags == 0 && tracer.badExclude == 0 && tracer.badTarget == 0);
            CT_EXPECT(r.calledActive == 2 && r.calledPassive == 1);
            CT_EXPECT(r.numActiveBefore == kVertices && r.numPassiveBefore == 0);
            CT_EXPECT(r.numActiveAfter == r.numActiveBefore && r.numPassiveAfter == r.numPassiveBefore); // (c) the state's own counts are back
            CT_EXPECT(r.varyingsUnchanged);

            // the same batch through a surface's illuminance: the passive point's Ci stays what it was, the active ones get (b)
            CAttributes *attributes2 = new CAttributes();
            attributes2->addLight(constLight);
            CAreaDouble sampler2;
            sampler2.bound = sampler.bound;
            sampler2.info = sampler.info;
            const SRun viaSurface = runSurface(s, ctx, sum, attributes2, &sampler2, 0, mixed);
            for (int c = 0; c < 3; c++) {
                CT_EXPECT_NEAR(viaSurface.ci[3 * 0 + c], (1.0f + c) / kPdf[0]);
                CT_EXPECT_NEAR(viaSurface.ci[3 * 2 + c], (1.0f + c) / kPdf[2]);
                CT_EXPECT(viaSurface.ci[3 * 1 + c] == -1.0f); // the harness's initial Ci: the passive point is never executed
            }
            CT_EXPECT(viaSurface.numActiveAfter == viaSurface.numActiveBefore && viaSurface.numPassiveAfter == viaSurface.numPassiveBefore);
        }
        // (d) every point passive: no entry, no probe, and the call returns
        {
            CAttributes *attributes = new CAttributes();
            attributes->addLight(constLight);
            CAreaDouble sampler;
            sampler.bound = identityOf(constLight);
            sampler.info.bound = true;
            sampler.info.sceneSamples = 3;
            sampler.info.sceneMode = AreaLightMode::Full;
            sampler.info.totalArea = 4.0;
            tracer.expectedExclude = identityOf(constLight);
            tracer.blocked = never;
            CAttributes *classicAttributes = new CAttributes();
            classicAttributes->addLight(constLight);
            const SDirectRun classic = runDirect(s, ctx, sum, classicAttributes, nullptr, none);
            CT_EXPECT(classic.entryCl.empty()); // a classic light with nothing active pushes no entry ...
            tracer.asked = 0;
            const SDirectRun r = runDirect(s, ctx, sum, attributes, &sampler, none);
            CT_EXPECT(r.entryCl.empty());       // ... and neither does a bound one, whatever its sample count
            CT_EXPECT(tracer.asked == 0);
            CT_EXPECT(r.calledActive == 0 && r.calledPassive == kVertices);
            CT_EXPECT(r.numActiveAfter == r.numActiveBefore && r.numPassiveAfter == r.numPassiveBefore);
        }
    }

    // ---- (11) __nondiffuse and __nonspecular on a bound light, against the same light run as a classic one -----------------------------
    {
        // The light sits at its `from` parameter and ignores the sampled point, so with one sample of density 1 the bound light's entry is the
        // classic light's (the same Cl and L): what the receiver's diffuse() and specular() make of it is the classic result bit for bit.
        // The receivers load in the run's format: #126 concerns a .slo surface whose only lighting call is illuminance(P), and these call diffuse() and specular().
        // Under the JIT (.slo light) the output variables __nondiffuse and __nonspecular have no effect for an illuminate() light, a classic one as much as a bound
        // one: jitIlluminateEnd saves Cl and -L and not the extras that diffuse() and specular() read. So the parity is asserted in both formats and the
        // scaling (0 the full result, 0.5 half of it, 1 none) for .rslo only; the .slo scaling check is pending a JIT engine issue (number to be added here).
        CProgrammableShaderInstance *diffuse = s.load("ca_surface_diffuse", SL_SURFACE);
        CProgrammableShaderInstance *specular = s.load("ca_surface_specular", SL_SURFACE);
        CT_EXPECT(diffuse != nullptr && specular != nullptr);
        auto pairRun = [&](CProgrammableShaderInstance *light, CProgrammableShaderInstance *receiver, SRun &bound, SRun &classic) {
            CAttributes *aC = new CAttributes();
            aC->addLight(light);
            classic = runSurface(s, ctx, receiver, aC, nullptr);
            CAttributes *aB = new CAttributes();
            aB->addLight(light);
            CAreaDouble sampler;
            sampler.bound = identityOf(light);
            sampler.info.bound = true;
            sampler.info.sceneSamples = 1;
            sampler.info.sceneMode = AreaLightMode::Full;
            sampler.info.totalArea = 4.0;
            sampler.pdf = kPdfOne;
            tracer.expectedExclude = identityOf(light);
            tracer.blocked = never;
            bound = runSurface(s, ctx, receiver, aB, &sampler);
            CT_EXPECT(sampler.sampleCalls == 1);
        };
        auto bitEqual = [&](const SRun &a, const SRun &b) { return a.ci.size() == b.ci.size() && std::memcmp(a.ci.data(), b.ci.data(), sizeof(float) * a.ci.size()) == 0; };
        if (diffuse != nullptr && specular != nullptr) {
            // varying __nondiffuse and __nonspecular: 0 the full result, 0.5 half of it (exact), 1 none; each the classic light's, bit for bit
            for (int kind = 0; kind < 2; kind++) {
                const char *light = kind == 0 ? "ca_light_nondiffuse" : "ca_light_nonspecular";
                const char *token = kind == 0 ? "float nondiffuse" : "float nonspecular";
                CProgrammableShaderInstance *receiver = kind == 0 ? diffuse : specular;
                std::vector<float> full;
                for (float v : {0.0f, 0.5f, 1.0f}) {
                    SRun b, c;
                    pairRun(loadLightParam(light, token, v), receiver, b, c);
                    CT_EXPECT(bitEqual(b, c));
                    if (v == 0.0f)
                        full = c.ci;
                    for (size_t k = 0; k < c.ci.size() && !jit; k++) { // the scaling: .rslo only, see above
                        if (v == 0.0f)
                            CT_EXPECT(c.ci[k] > 0.0f); // there is a diffuse or a specular result to scale
                        else
                            CT_EXPECT(b.ci[k] == (1.0f - v) * full[k]); // exact for 0.5 and 1
                    }
                }
            }
            // a uniform __nondiffuse and __nonspecular (the light's step is 0), both 1: the same result as the classic light, and for .rslo the light is skipped
            for (int kind = 0; kind < 2; kind++) {
                SRun b, c;
                pairRun(loadLightParam(kind == 0 ? "ca_light_nondiffuse_uniform" : "ca_light_nonspecular_uniform", nullptr, 0.0f), kind == 0 ? diffuse : specular, b, c);
                CT_EXPECT(bitEqual(b, c));
                for (size_t k = 0; k < c.ci.size() && !jit; k++) // skipped: .rslo only, see above
                    CT_EXPECT(b.ci[k] == 0.0f && c.ci[k] == 0.0f);
            }
        }
    }

    ctx->setTracer(nullptr);
    delete ctx;
    return contract_test::finish();
}
