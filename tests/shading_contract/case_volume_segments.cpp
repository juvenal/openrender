/**
 * tests/shading_contract/case_volume_segments.cpp
 *
 * ShadingContract_VolumeSegments{Interp,Jit} (spec 056, T013 to T016; US1 to US4, FR-001 to FR-010, FR-014).
 *
 * The segment step of research R2 and the bundles of data-model.md, driven without a renderer scene: a level-entering
 * tracer (CSegmentTracer below) answers the rays that trace(), transmission(), gather() and environment("raytrace")
 * spawn, going through the bundle protocol exactly as CHider::traceBundle does (src/ri/hiders/trace.cpp:255-269 and
 * the transparency loop above it): enterRayLevel, postTraceAction(), postShade(n, rays, varying) for hits (in chunks
 * of at most maxGridSize), postShade(n, rays) for misses (all of them in one call), post(), the continuation loop
 * that resets t to C_INFINITY, leaveRayLevel; a bundle refused at the maximum depth gets postShade(numRays, rays)
 * and no postTraceAction().
 *
 * The volume shaders are contract-only (shaders/cv_*.sl): cv_segment returns I as its colour, cv_segment_p returns P,
 * cv_globals returns (length(E), time, 0), cv_counter a marker colour no canned answer has, cv_overfog the "over" form
 * of fog, cv_nested a trace() of its own. A surface (cv_surface_*) sends one ray per point: along +z for the first
 * point (dot(dir, N) = 1 for the state's N = (0,0,1): the exterior side), along -z for the second (the interior side)
 * and along +x for the third (dot = 0, the tie, which reads interior). What trace() returns is therefore the step's
 * result, read back from Ci.
 *
 * The case needs a shaded object with Interior and Exterior bound (the standalone harness has none), so it runs in
 * the rendered program. On the starting tree it is RED for the right reason: no step exists, every ray returns the
 * canned colour, so the first check that expects a segment vector fails. The checks that expect "no step" (probes,
 * a side with no shader, a refused bundle) pass today and must stay green.
 *
 * Not covered by design (validation.md): motion blur and depth-of-field endpoints. FR-005 (primitive hit mode) is
 * covered by the rendered scene volume-primitive-hitmode: this tracer never calls shade().
 */
#include "cases_056.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "case_common.h"
#include "contract_forwarder.h"
#include "recording_tracer.h"
#include "rendered_harness.h"

namespace {

    // Tolerances (tasks.md, Conventions): 0.0001 where the value contains exp(), 0.00001 for a pass-through value.
    // The contract values here are built from float arithmetic on a handful of operations; both are absolute.
    constexpr float kTolFog = 1e-4f;
    constexpr float kTolValue = 1e-5f;

    bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

    const float kCanned[3] = {0.25f, 0.5f, 0.75f}; // the colour of an ordinary canned hit

    // What the tracer says about one ray of one pass
    struct SAnswer {
            bool hit = false;
            float t = 0;           // the hit distance (absolute along the ray)
            float ci[3] = {0, 0, 0};
            float oi[3] = {1, 1, 1};
            float clipT = -1;      // for a miss: a finite t the ray starts with (a clip plane), < 0 = leave C_INFINITY
    };

    // One bundle request as the tracer saw it
    struct SRequest {
            int depthAtEntry = 0;                    // the context's ray depth when the request arrived
            bool volume = false;                     // the bundle carried a volume shader (postShader)
            bool refused = false;                    // enterRayLevel refused it (maximum depth)
            std::vector<STraceRay> rays;             // the inputs of the first pass
    };

    // A level-entering tracer (T013). Hits are shaded by a canned answer, never by shade().
    class CSegmentTracer : public openrender::shading::ITracer {
        public:
            std::function<SAnswer(const CRay &, int pass)> answer;
            CSurface *hitObject = nullptr;
            int maxDepth = 8;
            std::vector<SRequest> requests;
            int rayRequests = 0; // traceRay() calls (probes and the irradiance cache)

            void traceRay(CShadingContext &, CRay *ray) override {
                ++rayRequests;
                const SAnswer a = answer(*ray, 0);
                if (a.hit) {
                    ray->object = hitObject;
                    ray->t = a.t;
                    ray->u = ray->v = 0.5f;
                    ray->N[0] = ray->N[1] = 0;
                    ray->N[2] = 1;
                }
                else {
                    ray->object = nullptr;
                }
            }

            void traceBundle(CShadingContext &ctx, CRayBundle *bundle) override {
                SRequest q;
                q.depthAtEntry = ctx.rayDepth();
                q.volume = bundle->postShader != nullptr;
                for (int i = 0; i < bundle->numRays; i++)
                    q.rays.push_back(copyRayInputs(bundle->rays[i]));

                CShadingContext::CRayLevel saved;
                if (!ctx.enterRayLevel(*bundle, maxDepth, saved)) {
                    q.refused = true;
                    requests.push_back(q);
                    bundle->postShade(bundle->numRays, bundle->rays); // the refusal form: no postTraceAction()
                    return;
                }
                requests.push_back(q);

                int numRays = bundle->numRays;
                CRay **rays = bundle->rays;
                int pass = 0;
                while (true) {
                    std::vector<CRay *> hits, misses;
                    std::vector<SAnswer> hitAnswers;
                    for (int i = 0; i < numRays; i++) {
                        CRay *r = rays[i];
                        const SAnswer a = answer(*r, pass);
                        if (a.hit) {
                            r->object = hitObject;
                            r->t = a.t;
                            hits.push_back(r);
                            hitAnswers.push_back(a);
                        }
                        else {
                            r->object = nullptr;
                            if (a.clipT >= 0)
                                r->t = a.clipT;
                            misses.push_back(r);
                        }
                    }
                    if (bundle->postTraceAction() == FALSE)
                        break;
                    float **varying = ctx.currentShadingState->varying;
                    const int mgs = CRenderer::maxGridSize;
                    for (size_t first = 0; first < hits.size(); first += (size_t)mgs) {
                        const size_t n = std::min((size_t)mgs, hits.size() - first);
                        for (size_t k = 0; k < n; k++) {
                            memcpy(varying[VARIABLE_CI] + 3 * k, hitAnswers[first + k].ci, 3 * sizeof(float));
                            memcpy(varying[VARIABLE_OI] + 3 * k, hitAnswers[first + k].oi, 3 * sizeof(float));
                        }
                        bundle->postShade((int)n, hits.data() + first, varying);
                    }
                    if (!misses.empty())
                        bundle->postShade((int)misses.size(), misses.data()); // all the misses of a pass in one call
                    bundle->post();

                    if ((numRays = bundle->numRays) == 0)
                        break;
                    rays = bundle->rays;
                    for (int i = 0; i < numRays; i++) { // the continuation loop of trace.cpp:230-240
                        rays[i]->tmin = rays[i]->t + C_EPSILON;
                        rays[i]->t = C_INFINITY;
                    }
                    ++pass;
                }
                ctx.leaveRayLevel(saved);
            }
    };

    // The rendered program's machinery for one case
    struct SSetup {
            CRenderedSession s;
            CContractForwarder fwd;
            CSegmentTracer tracer;
            CRProbeContext *ctx;

            explicit SSetup(bool jit) : s(jit), fwd(s.original) {
                fwd.tracerOverride = &tracer;
                tracer.hitObject = s.object;
                ctx = s.makeContext(&fwd);
            }
            ~SSetup() { delete ctx; }

            CProgrammableShaderInstance *load(const char *name, unsigned int type) { return s.load(name, (int)type); }

            void bindInterior(CProgrammableShaderInstance *v) { s.attributes->bindInterior(v != nullptr ? ShaderGroupRef::retain(v) : ShaderGroupRef()); }
            void bindExterior(CProgrammableShaderInstance *v) { s.attributes->bindExterior(v != nullptr ? ShaderGroupRef::retain(v) : ShaderGroupRef()); }

            // Run a surface on three points (the state of the harness: P = 0.1*(i+1)*(1,2,3), N = (0,0,1), s = 0.1*(i+1)).
            // `time` is the ray time of the points. Returns Ci and Oi of the points.
            SHarnessResult run(CProgrammableShaderInstance *surface, const float time[3] = nullptr) {
                tracer.requests.clear();
                tracer.rayRequests = 0;
                CRenderedSession::setupState(ctx, 3, s.object);
                CShadingState *ss = ctx->currentShadingState;
                for (int i = 0; i < 3; i++)
                    ss->varying[VARIABLE_TIME][i] = time != nullptr ? time[i] : 0.0f;
                ss->currentShaderInstance = surface;
                CMemPage *localMemory = NULL;
                memoryInit(localMemory);
                float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);
                ctx->execute(surface, locals);
                SHarnessResult r;
                for (int i = 0; i < 3; i++) {
                    r.ci.insert(r.ci.end(), ss->varying[VARIABLE_CI] + 3 * i, ss->varying[VARIABLE_CI] + 3 * i + 3);
                    r.oi.insert(r.oi.end(), ss->varying[VARIABLE_OI] + 3 * i, ss->varying[VARIABLE_OI] + 3 * i + 3);
                }
                ss->lights = nullptr;
                ss->freeLights = nullptr;
                ss->currentShaderInstance = nullptr;
                memoryTini(localMemory);
                return r;
            }
    };

    // The rays of the first pass of all the requests, ordered by from.x: the i-th is the ray of point i (the points'
    // P.x grow with i and every builtin sends one ray per point)
    std::vector<STraceRay> raysByPoint(const std::vector<SRequest> &requests) {
        std::vector<STraceRay> rays;
        for (const SRequest &q : requests)
            rays.insert(rays.end(), q.rays.begin(), q.rays.end());
        std::sort(rays.begin(), rays.end(), [](const STraceRay &a, const STraceRay &b) { return a.from[0] < b.from[0]; });
        return rays;
    }

    // The canned answer of the ordinary hit: opaque, a distance that depends on the ray's origin
    float hitT(float fromX) { return 1.5f + 2.0f * fromX; }
    SAnswer opaqueHit(const CRay &r) {
        SAnswer a;
        a.hit = true;
        a.t = hitT(r.from[0]);
        memcpy(a.ci, kCanned, sizeof(kCanned));
        return a;
    }
    SAnswer missAnswer(const CRay &) { return SAnswer(); }

    // The segment vector and endpoint of a ray that ends at e (starting at s = 0)
    void segment(const STraceRay &r, float e, float I[3], float P[3]) {
        for (int k = 0; k < 3; k++) {
            I[k] = e * r.dir[k];
            P[k] = r.from[k] + e * r.dir[k];
        }
    }

    // The far exit of a ray from an axis-aligned box in double precision, clamped to >= 0 (the step's rule, R5);
    // 0 for a ray that never enters it or an empty box
    float boundExit(const STraceRay &r, const float bmin[3], const float bmax[3]) {
        double tNear = 0, tFar = 1e300;
        for (int k = 0; k < 3; k++) {
            if (bmin[k] > bmax[k])
                return 0;
            const double o = r.from[k], d = r.dir[k];
            if (d == 0) {
                if (o < bmin[k] || o > bmax[k])
                    return 0;
                continue;
            }
            double t0 = (bmin[k] - o) / d, t1 = (bmax[k] - o) / d;
            if (t0 > t1)
                std::swap(t0, t1);
            tNear = std::max(tNear, t0);
            tFar = std::min(tFar, t1);
        }
        if (tNear > tFar)
            return 0;
        return (float)std::max(0.0, tFar);
    }

    // Expected colour of point i when the side's volume is cv_segment (the ray goes along +z: point 0 only) or
    // cv_segment_p (the others), or none: the unattenuated canned colour
    bool expectColour(const SHarnessResult &r, int i, const float want[3], float tol, const char *what) {
        bool ok = true;
        for (int k = 0; k < 3; k++)
            ok = ok && near(r.ciAt(i, k), want[k], tol);
        if (!ok)
            fprintf(stderr, "  point %d (%s): got (%g %g %g) wanted (%g %g %g)\n", i, what, r.ciAt(i, 0), r.ciAt(i, 1), r.ciAt(i, 2), want[0], want[1], want[2]);
        return ok;
    }

    CProgrammableShaderInstance *must(SSetup &c, const char *name, unsigned int type) {
        CProgrammableShaderInstance *p = c.load(name, type);
        CT_EXPECT(p != nullptr);
        return p;
    }

} // namespace

int caseVolumeSegments(bool jit) {
    const char *fmt = jit ? "slo" : "rslo";
    SSetup c(jit);
    CProgrammableShaderInstance *seg = must(c, "cv_segment", SL_ATMOSPHERE);
    CProgrammableShaderInstance *segP = must(c, "cv_segment_p", SL_ATMOSPHERE);
    CProgrammableShaderInstance *glob = must(c, "cv_globals", SL_ATMOSPHERE);
    CProgrammableShaderInstance *nested = must(c, "cv_nested", SL_ATMOSPHERE);
    CProgrammableShaderInstance *marker = must(c, "cv_counter", SL_ATMOSPHERE);
    CProgrammableShaderInstance *overfog = must(c, "cv_overfog", SL_ATMOSPHERE);
    CProgrammableShaderInstance *sTrace = must(c, "cv_surface_trace", SL_SURFACE);
    CProgrammableShaderInstance *sTrans = must(c, "cv_surface_transmission", SL_SURFACE);
    CProgrammableShaderInstance *sGather = must(c, "cv_surface_gather", SL_SURFACE);
    CProgrammableShaderInstance *sEnv = must(c, "cv_surface_environment", SL_SURFACE);
    CProgrammableShaderInstance *sProbe = must(c, "cv_surface_visibility", SL_SURFACE);
    CProgrammableShaderInstance *sOcc = must(c, "cv_surface_occlusion", SL_SURFACE);
    CProgrammableShaderInstance *sInd = must(c, "cv_surface_indirect", SL_SURFACE);
    if (!(seg && segP && glob && nested && marker && overfog && sTrace && sTrans && sGather && sEnv && sProbe && sOcc && sInd))
        return contract_test::finish();

    c.tracer.answer = [](const CRay &r, int) { return opaqueHit(r); };

    // The bound of the services double's world: exact values, restored at the end
    vector savedMin, savedMax;
    movvv(savedMin, CRenderer::worldBmin);
    movvv(savedMax, CRenderer::worldBmax);
    auto setBound = [](float lo, float hi) {
        initv(CRenderer::worldBmin, lo, lo, lo);
        initv(CRenderer::worldBmax, hi, hi, hi);
    };

    // ---- the matrix of a side-split check ------------------------------------------------------------------------------
    // Point 0 goes along +z (the exterior side of N = (0,0,1)), point 1 along -z (the interior side), point 2 along +x
    // (dot(dir, N) = 0, the tie: it reads interior)
    enum EKind { NONE, SEG_I, SEG_P };
    auto bindKind = [&](EKind ext, EKind in) {
        auto pick = [&](EKind k) { return k == SEG_I ? seg : (k == SEG_P ? segP : nullptr); };
        c.bindExterior(pick(ext));
        c.bindInterior(pick(in));
    };
    // `base` is what an unattenuated ray returns (the canned colour for a hit, black for a miss);
    // `end(i, ray)` is the endpoint distance e of point i's segment (it starts at 0)
    auto checkSides = [&](const char *what, const SHarnessResult &r, const std::vector<STraceRay> &rays, EKind ext, EKind in, const float base[3],
                          const std::function<float(int, const STraceRay &)> &end) {
        CT_EXPECT(rays.size() == 3);
        if (rays.size() != 3)
            return;
        for (int i = 0; i < 3; i++) {
            const EKind kind = rays[i].dir[2] > 0 ? ext : in;
            float I[3], P[3], want[3];
            segment(rays[i], end(i, rays[i]), I, P);
            for (int k = 0; k < 3; k++)
                want[k] = kind == SEG_I ? I[k] : (kind == SEG_P ? P[k] : base[k]);
            CT_EXPECT(expectColour(r, i, want, kTolValue, what));
        }
    };
    const float kBlack[3] = {0, 0, 0};
    auto hitEnd = [](int, const STraceRay &ray) { return hitT(ray.from[0]); };

    // ---- T014: hits. Exterior = cv_segment (Ci = I), Interior = cv_segment_p (Ci = P) --------------------------------
    {
        CProgrammableShaderInstance *surfaces[3] = {sTrace, sEnv, sGather};
        const char *names[3] = {"trace", "environment", "gather"};
        for (int b = 0; b < 3; b++) {
            // trace(), environment("raytrace") and gather() select by the sign of dir . N alike (FR-001, FR-002; Q2)
            printf("  %s: a hit gets I on the exterior side and P on the interior side (%s)\n", names[b], fmt);
            c.tracer.answer = [](const CRay &r, int) { return opaqueHit(r); };
            bindKind(SEG_I, SEG_P);
            const SHarnessResult r = c.run(surfaces[b]);
            checkSides("hit, both sides", r, raysByPoint(c.tracer.requests), SEG_I, SEG_P, kCanned, hitEnd);
            if (b == 0) {
                // trace() splits the rays into one bundle per side, each with a volume shader (data-model.md)
                CT_EXPECT(c.tracer.requests.size() == 2);
                for (const SRequest &q : c.tracer.requests)
                    CT_EXPECT(q.volume);
            }

            // A surface with only one of the two shaders gets no step on the other side (the missing side is not an error)
            printf("  %s: a side with no shader gets no step (%s)\n", names[b], fmt);
            bindKind(SEG_I, NONE);
            checkSides("hit, interior unbound", c.run(surfaces[b]), raysByPoint(c.tracer.requests), SEG_I, NONE, kCanned, hitEnd);
            bindKind(NONE, SEG_P);
            checkSides("hit, exterior unbound", c.run(surfaces[b]), raysByPoint(c.tracer.requests), NONE, SEG_P, kCanned, hitEnd);
        }

        // A7: a matte spawning surface still attenuates the rays it spawns
        printf("  A7: a matte spawning surface still attenuates (%s)\n", fmt);
        c.s.attributes->flags |= ATTRIBUTES_FLAGS_MATTE;
        bindKind(SEG_I, SEG_P);
        checkSides("matte spawning surface", c.run(sTrace), raysByPoint(c.tracer.requests), SEG_I, SEG_P, kCanned, hitEnd);
        c.s.attributes->flags &= ~ATTRIBUTES_FLAGS_MATTE;
    }

    // ---- T014: E, time and every other global (contracts/segment-step.md, section 3) ----------------------------------
    {
        printf("  the globals of a segment: E = 0, time = the ray's, the rest 0 (%s)\n", fmt);
        const float times[3] = {0.25f, 0.5f, 0.75f};
        c.bindExterior(glob);
        c.bindInterior(glob);
        const SHarnessResult r = c.run(sTrace, times);
        const std::vector<STraceRay> rays = raysByPoint(c.tracer.requests);
        CT_EXPECT(rays.size() == 3);
        if (rays.size() == 3) {
            for (int i = 0; i < 3; i++) {
                const float want[3] = {0.0f, times[i], 0.0f};
                CT_EXPECT(near(rays[i].time, times[i], 0.0f));
                CT_EXPECT(expectColour(r, i, want, kTolValue, "globals"));
            }
        }
    }

    // ---- T014: misses ---------------------------------------------------------------------------------------------------
    {
        c.tracer.answer = [](const CRay &r, int) { return missAnswer(r); };
        bindKind(SEG_I, SEG_P);

        printf("  trace: a miss with a finite ray t ends at it (%s)\n", fmt);
        c.tracer.answer = [](const CRay &, int) {
            SAnswer a;
            a.clipT = 7.5f;
            return a;
        };
        checkSides("miss, clipped", c.run(sTrace), raysByPoint(c.tracer.requests), SEG_I, SEG_P, kBlack, [](int, const STraceRay &) { return 7.5f; });
        c.tracer.answer = [](const CRay &r, int) { return missAnswer(r); };

        printf("  trace: a miss with nothing finite ends at the world-bound exit (%s)\n", fmt);
        setBound(-100, 100);
        {
            const float bmin[3] = {-100, -100, -100}, bmax[3] = {100, 100, 100};
            checkSides("miss, bound exit", c.run(sTrace), raysByPoint(c.tracer.requests), SEG_I, SEG_P, kBlack,
                       [&](int, const STraceRay &ray) { return boundExit(ray, bmin, bmax); });
        }

        printf("  trace: a ray that never enters the bound, or an empty world, gives I = 0 (%s)\n", fmt);
        setBound(4, 6); // the points' x are 0.1 to 0.3: the rays run beside the box
        {
            const float bmin[3] = {4, 4, 4}, bmax[3] = {6, 6, 6};
            checkSides("miss, outside the bound", c.run(sTrace), raysByPoint(c.tracer.requests), SEG_I, SEG_P, kBlack,
                       [&](int, const STraceRay &ray) { return boundExit(ray, bmin, bmax); });
        }
        initv(CRenderer::worldBmin, 1, 1, 1);
        initv(CRenderer::worldBmax, -1, -1, -1); // an empty bound
        checkSides("miss, empty bound", c.run(sTrace), raysByPoint(c.tracer.requests), SEG_I, SEG_P, kBlack, [](int, const STraceRay &) { return 0.0f; });

        // More misses than maxGridSize() in one postShade() call are processed in chunks (data-model.md, shadeVolumeSegments):
        // the grid size is lowered after the context's buffers exist
        printf("  trace: more misses than maxGridSize() in one call are processed in chunks (%s)\n", fmt);
        setBound(-100, 100);
        const int savedMgs = CRenderer::maxGridSize;
        CRenderer::maxGridSize = 2;
        {
            const float bmin[3] = {-100, -100, -100}, bmax[3] = {100, 100, 100};
            checkSides("miss, chunks", c.run(sTrace), raysByPoint(c.tracer.requests), SEG_I, SEG_P, kBlack,
                       [&](int, const STraceRay &ray) { return boundExit(ray, bmin, bmax); });
        }
        CRenderer::maxGridSize = savedMgs;

        printf("  gather: a miss ends at maxdist, and the step's result is recorded in surface:Ci only (%s)\n", fmt);
        setBound(-100, 100);
        bindKind(NONE, NONE);
        const SHarnessResult base = c.run(sGather); // the starting values of the other outputs: ray:direction and the miss count
        bindKind(SEG_I, SEG_P);
        const SHarnessResult r = c.run(sGather);
        checkSides("gather miss, maxdist", r, raysByPoint(c.tracer.requests), SEG_I, SEG_P, kBlack, [](int, const STraceRay &) { return 5.0f; });
        for (int i = 0; i < 3; i++) // ray:direction and the miss count (Oi) are unchanged (FR-004, FR-007)
            for (int k = 0; k < 3; k++)
                CT_EXPECT(near(r.oiAt(i, k), base.oiAt(i, k), 0.0f));

        printf("  gather: a hit records the step's result and leaves the other outputs alone (%s)\n", fmt);
        c.tracer.answer = [](const CRay &r, int) { return opaqueHit(r); };
        bindKind(NONE, NONE);
        const SHarnessResult baseHit = c.run(sGather);
        bindKind(SEG_I, SEG_P);
        const SHarnessResult rh = c.run(sGather);
        checkSides("gather hit", rh, raysByPoint(c.tracer.requests), SEG_I, SEG_P, kCanned, hitEnd);
        for (int i = 0; i < 3; i++)
            for (int k = 0; k < 3; k++)
                CT_EXPECT(near(rh.oiAt(i, k), baseHit.oiAt(i, k), 0.0f));

        printf("  transmission: a miss ends at its destination (%s)\n", fmt);
        c.tracer.answer = [](const CRay &r, int) { return missAnswer(r); };
        c.bindExterior(overfog);
        c.bindInterior(overfog);
        {
            // cv_overfog, distance 2: transmission() returns 1 - Oi' = T = exp(-l/2), l the segment to the destination
            // (the ray's initial t; the hider resets a continued ray's t, which is why the bundle records the destination)
            const SHarnessResult rt = c.run(sTrans);
            const std::vector<STraceRay> rays = raysByPoint(c.tracer.requests);
            CT_EXPECT(rays.size() == 3);
            if (rays.size() == 3)
                for (int i = 0; i < 3; i++) {
                    const float want = std::exp(-rays[i].t / 2.0f);
                    for (int k = 0; k < 3; k++)
                        CT_EXPECT(near(rt.ciAt(i, k), want, kTolFog));
                }
        }
    }

    // ---- T015: depth ----------------------------------------------------------------------------------------------------
    {
        printf("  a bundle refused at the maximum depth runs no volume and returns today's result (%s)\n", fmt);
        c.tracer.answer = [](const CRay &r, int) { return opaqueHit(r); };
        c.tracer.maxDepth = 0;
        bindKind(SEG_I, SEG_P);
        {
            const SHarnessResult r = c.run(sTrace);
            for (int i = 0; i < 3; i++)
                CT_EXPECT(expectColour(r, i, kBlack, 0.0f, "refused trace: black"));
            for (const SRequest &q : c.tracer.requests)
                CT_EXPECT(q.refused);
            const SHarnessResult rt = c.run(sTrans);
            for (int i = 0; i < 3; i++) {
                const float one[3] = {1, 1, 1};
                CT_EXPECT(expectColour(rt, i, one, 0.0f, "refused transmission: fully transmitted"));
            }
        }

        // cv_nested traces once from inside the step. At maxDepth 1 the outer bundle is allowed (depth 0 to 1) and the nested
        // one refused (depth 1 is the maximum): black. At maxDepth 2 it is allowed, answered with the canned colour, and the
        // nested request carries no volume (FR-009). The outer result is what the volume shader returned.
        printf("  a trace() inside a volume shader carries no volume and counts toward the depth (%s)\n", fmt);
        bindKind(NONE, NONE);
        c.bindExterior(nested);
        c.bindInterior(nested);
        c.tracer.maxDepth = 1;
        {
            const SHarnessResult r = c.run(sTrace);
            for (int i = 0; i < 3; i++)
                CT_EXPECT(expectColour(r, i, kBlack, 0.0f, "nested trace refused at the maximum depth"));
        }
        c.tracer.maxDepth = 2;
        {
            const SHarnessResult r = c.run(sTrace);
            for (int i = 0; i < 3; i++)
                CT_EXPECT(expectColour(r, i, kCanned, kTolValue, "nested trace allowed"));
            // one outer request (both sides bind the same shader, so one bundle) and the nested one, which carries no volume and sits one level deeper
            CT_EXPECT(c.tracer.requests.size() >= 2);
            for (const SRequest &q : c.tracer.requests)
                if (q.depthAtEntry >= 1)
                    CT_EXPECT(!q.volume);
        }
        c.tracer.maxDepth = 8;
    }

    // ---- T015: segments and compositing (FR-008, research R4) -----------------------------------------------------------
    {
        // A transparent hit (Oi 0.5, colour C1) at t1, then an opaque one (colour C2) at t1 + 1: with the over-form volume
        // cv_overfog (distance D, background bg) the colour is d1*bg + T1*C1 + T1*(1 - O1)*(d2*bg + T2*C2)
        printf("  trace: two segments composite as the whole path (%s)\n", fmt);
        const float C1[3] = {0.2f, 0.4f, 0.6f}, C2[3] = {0.7f, 0.5f, 0.3f}, O1 = 0.5f;
        const float D = 2.0f, bg[3] = {0.1f, 0.3f, 0.9f};
        c.tracer.answer = [&](const CRay &r, int pass) {
            SAnswer a;
            a.hit = true;
            if (pass == 0) {
                a.t = hitT(r.from[0]);
                memcpy(a.ci, C1, sizeof(C1));
                a.oi[0] = a.oi[1] = a.oi[2] = O1;
            }
            else {
                a.t = hitT(r.from[0]) + 1.0f; // an absolute distance along the same ray: the second segment is 1 long
                memcpy(a.ci, C2, sizeof(C2));
            }
            return a;
        };
        c.bindExterior(overfog);
        c.bindInterior(overfog);
        {
            const SHarnessResult r = c.run(sTrace);
            const std::vector<STraceRay> rays = raysByPoint(c.tracer.requests);
            CT_EXPECT(rays.size() == 3);
            if (rays.size() == 3)
                for (int i = 0; i < 3; i++) {
                    const float l1 = hitT(rays[i].from[0]), l2 = 1.0f;
                    const float T1 = std::exp(-l1 / D), T2 = std::exp(-l2 / D), d1 = 1 - T1, d2 = 1 - T2;
                    float want[3];
                    for (int k = 0; k < 3; k++)
                        want[k] = d1 * bg[k] + T1 * C1[k] + T1 * (1 - O1) * (d2 * bg[k] + T2 * C2[k]);
                    CT_EXPECT(expectColour(r, i, want, kTolFog, "two-segment composite"));
                }
        }

        // transmission(): a transparent hit (O 0.5) at t1, then the ray runs to its destination: 1 - Oi' accumulated is
        // 0.5 * T1 * T2 = 0.5 * exp(-(destination)/D); a continued ray's t is reset, so the destination is the recorded one
        printf("  transmission: two segments, the last ends at the destination (%s)\n", fmt);
        c.tracer.answer = [&](const CRay &r, int pass) {
            SAnswer a;
            if (pass == 0) {
                a.hit = true;
                a.t = 0.8f + r.from[0];
                a.oi[0] = a.oi[1] = a.oi[2] = O1;
            }
            return a; // pass 1: a miss
        };
        {
            const SHarnessResult r = c.run(sTrans);
            const std::vector<STraceRay> rays = raysByPoint(c.tracer.requests);
            CT_EXPECT(rays.size() == 3);
            if (rays.size() == 3)
                for (int i = 0; i < 3; i++) {
                    const float want = (1 - O1) * std::exp(-rays[i].t / D);
                    for (int k = 0; k < 3; k++)
                        CT_EXPECT(near(r.ciAt(i, k), want, kTolFog));
                }
        }
        c.tracer.answer = [](const CRay &r, int) { return opaqueHit(r); };
    }

    // ---- T016: probes and the other ray kinds run no volume ----------------------------------------------------------------
    {
        // visibility(), the float form of trace(), occlusion() and indirectdiffuse() are not radiance segments (FR-010, A8):
        // the result is the same with and without volumes bound, and no bundle they send carries a volume shader
        CProgrammableShaderInstance *others[3] = {sProbe, sOcc, sInd};
        const char *names[3] = {"visibility and float trace", "occlusion", "indirectdiffuse"};
        for (int b = 0; b < 3; b++) {
            printf("  %s: no volume step (%s)\n", names[b], fmt);
            bindKind(NONE, NONE);
            c.bindExterior(nullptr);
            c.bindInterior(nullptr);
            const SHarnessResult without = c.run(others[b]);
            c.bindExterior(marker);
            c.bindInterior(marker);
            const SHarnessResult with = c.run(others[b]);
            for (int i = 0; i < 3; i++)
                for (int k = 0; k < 3; k++)
                    CT_EXPECT(near(with.ciAt(i, k), without.ciAt(i, k), 0.0f));
            for (const SRequest &q : c.tracer.requests)
                CT_EXPECT(!q.volume);
            if (b == 0)
                CT_EXPECT(c.tracer.rayRequests + (int)c.tracer.requests.size() > 0); // the probes did reach the tracer
        }
    }

    movvv(CRenderer::worldBmin, savedMin);
    movvv(CRenderer::worldBmax, savedMax);
    return contract_test::finish();
}
