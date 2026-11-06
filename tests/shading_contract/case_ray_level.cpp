/**
 * tests/shading_contract/case_ray_level.cpp
 *
 * ShadingContract_RayLevel (spec 029, T037; T-T2; FR-017, edge case "re-entrancy"). Standalone.
 *
 * Going one ray level deeper is the context's public pair enterRayLevel()/leaveRayLevel() (shading-context.md), the one
 * place the depth rule lives, so a hider outside the class can do what traceEx() did. This case pins them:
 *   - enterRayLevel() below the limit returns true, adds one to rayDepth(), takes a fresh shading state whose postShader is
 *     the bundle's, and records the previous state and ray label in the CRayLevel (the label is read back through the
 *     next level's saved label: the context has no public accessor for it, and needs none);
 *   - at the limit it returns false and changes nothing: the depth, the current state and the free list stay;
 *   - leaveRayLevel() restores, in traceEx's order: depth - 1, the state back on the free list (the next level reuses it),
 *     the label and the state of before;
 *   - a tracer applying the rule, as a hider will (enter, trace, leave; at the limit the miss path postShade(n, rays)
 *     and neither postTraceAction() nor post()), sees exactly one miss postShade at the limit.
 *
 * Red on the starting tree: a build failure (CShadingContext::enterRayLevel, leaveRayLevel, rayDepth and CRayLevel do not
 * exist; the rule is the body of the protected traceEx()).
 */
#include "cases_029.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "libshader/shading/tracer.h"
#include "ri/core/ray.h"

using namespace openrender::shading;

namespace {

    // The type of enterRayLevel()'s third parameter. shading-context.md declares CRayLevel among the context's public members
    // and hider.md uses it unqualified; the alias names it whichever of the two the header settles on
    template <class R, class C, class A, class B, class L> L rayLevelOf(R (C::*)(A, B, L &));
    using CRayLevelType = decltype(rayLevelOf(&CShadingContext::enterRayLevel));

    // A bundle that counts the protocol calls it receives
    class CCountingBundle final : public CRayBundle {
        public:
            int postTraceActions = 0, hitShades = 0, missShades = 0, posts = 0;
            int missRays = -1;

            CCountingBundle(const char *l, CShaderInstance *shader) {
                numRays = 2;
                rays = rayArray;
                rayArray[0] = &ray[0];
                rayArray[1] = &ray[1];
                label = l;
                last = 0;
                depth = 0;
                postShader = shader;
            }
            int postTraceAction() override {
                ++postTraceActions;
                return TRUE;
            }
            void postShade(int, CRay **, float **) override { ++hitShades; }
            void postShade(int n, CRay **) override {
                ++missShades;
                missRays = n;
            }
            void post() override {
                ++posts;
                numRays = 0; // one pass
            }

        private:
            CRay ray[2];
            CRay *rayArray[2];
    };

    // A tracer that applies the depth rule exactly as a hider's traceBundle() will
    class CDepthRuleTracer final : public ITracer {
        public:
            int maxDepth = 0;
            int traced = 0;       // bundles that were traced (the rule let them in)
            int depthSeen = -1;   // rayDepth() while tracing

            void traceRay(CShadingContext &, CRay *) override {}
            void traceBundle(CShadingContext &context, CRayBundle *bundle) override {
                CRayLevelType saved;
                if (!context.enterRayLevel(*bundle, maxDepth, saved)) {
                    bundle->postShade(bundle->numRays, bundle->rays);
                    return;
                }
                ++traced;
                depthSeen = context.rayDepth();
                if (bundle->postTraceAction() != FALSE) {
                    bundle->postShade(0, bundle->rays); // a miss pass for the canned rays
                    bundle->post();
                }
                context.leaveRayLevel(saved);
            }
    };

} // namespace

int caseRayLevel() {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        CShadingContext &ctx = *h.context;
        // A pointer is enough for a shader: nothing here executes it
        int shaderA = 0, shaderB = 0;
        CShaderInstance *postA = reinterpret_cast<CShaderInstance *>(&shaderA);
        CShaderInstance *postB = reinterpret_cast<CShaderInstance *>(&shaderB);

        // ---- enter below the limit ---------------------------------------------------------------------------
        printf("  enterRayLevel below the limit\n");
        CT_EXPECT(ctx.rayDepth() == 0);
        CShadingState *base = ctx.currentShadingState;
        CCountingBundle a("levelA", postA);
        CRayLevelType s1;
        CT_EXPECT(ctx.enterRayLevel(a, 3, s1));
        CT_EXPECT(ctx.rayDepth() == 1);
        CShadingState *level1 = ctx.currentShadingState;
        CT_EXPECT(level1 != nullptr && level1 != base);
        CT_EXPECT(s1.savedState == base);
        CT_EXPECT(level1->postShader == postA);

        // The next level saves the label this one set, which is how the label is observed
        CCountingBundle b("levelB", postB);
        CRayLevelType s2;
        CT_EXPECT(ctx.enterRayLevel(b, 3, s2));
        CT_EXPECT(ctx.rayDepth() == 2);
        CShadingState *level2 = ctx.currentShadingState;
        CT_EXPECT(level2 != level1 && level2 != base);
        CT_EXPECT(s2.savedState == level1);
        CT_EXPECT(s2.savedLabel != nullptr && strcmp(s2.savedLabel, "levelA") == 0);
        CT_EXPECT(level2->postShader == postB);

        // ---- leave, in traceEx's order ----------------------------------------------------------------------------
        printf("  leaveRayLevel restores\n");
        ctx.leaveRayLevel(s2);
        CT_EXPECT(ctx.rayDepth() == 1);
        CT_EXPECT(ctx.currentShadingState == level1);
        ctx.leaveRayLevel(s1);
        CT_EXPECT(ctx.rayDepth() == 0);
        CT_EXPECT(ctx.currentShadingState == base);

        // The states went back on the free list: the next levels take them again, most recent first
        CCountingBundle c("levelC", postA);
        CRayLevelType s3;
        CT_EXPECT(ctx.enterRayLevel(c, 3, s3));
        CT_EXPECT(ctx.currentShadingState == level1);
        ctx.leaveRayLevel(s3);
        CT_EXPECT(ctx.currentShadingState == base && ctx.rayDepth() == 0);

        // ---- at the limit -------------------------------------------------------------------------------------------
        printf("  enterRayLevel at the limit\n");
        CCountingBundle d("levelD", postB);
        CRayLevelType s4;
        CT_EXPECT(!ctx.enterRayLevel(d, 0, s4)); // rayDepth() == 0, limit 0
        CT_EXPECT(ctx.rayDepth() == 0);
        CT_EXPECT(ctx.currentShadingState == base);
        CT_EXPECT(d.postTraceActions == 0 && d.missShades == 0); // the caller makes the miss call, not the context

        // One level in, a limit of 1 refuses the next
        CRayLevelType s5;
        CT_EXPECT(ctx.enterRayLevel(a, 1, s5));
        CRayLevelType s6;
        CShadingState *inLevel = ctx.currentShadingState;
        CT_EXPECT(!ctx.enterRayLevel(b, 1, s6));
        CT_EXPECT(ctx.rayDepth() == 1 && ctx.currentShadingState == inLevel);
        ctx.leaveRayLevel(s5);
        CT_EXPECT(ctx.rayDepth() == 0 && ctx.currentShadingState == base);

        // ---- a tracer applying the rule ------------------------------------------------------------------------------
        printf("  a tracer at the limit sees one miss postShade and no postTraceAction\n");
        CDepthRuleTracer tracer;
        tracer.maxDepth = 0;
        CCountingBundle atLimit("limit", postA);
        tracer.traceBundle(ctx, &atLimit);
        CT_EXPECT(tracer.traced == 0);
        CT_EXPECT(atLimit.missShades == 1 && atLimit.missRays == 2);
        CT_EXPECT(atLimit.postTraceActions == 0 && atLimit.hitShades == 0 && atLimit.posts == 0);
        CT_EXPECT(ctx.rayDepth() == 0 && ctx.currentShadingState == base);

        tracer.maxDepth = 2;
        CCountingBundle below("below", postA);
        tracer.traceBundle(ctx, &below);
        CT_EXPECT(tracer.traced == 1 && tracer.depthSeen == 1);
        CT_EXPECT(below.postTraceActions == 1 && below.posts == 1);
        CT_EXPECT(ctx.rayDepth() == 0 && ctx.currentShadingState == base);
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
