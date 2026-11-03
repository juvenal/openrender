/**
 * tests/shading_contract/case_illumination_hook_interface.cpp
 *
 * ShadingContract_IlluminationHookInterface{Interp,Jit} (spec 029, T035; T-H1; FR-013, US5 AS4).
 *
 * The photon pass's hooks into illuminate() and solar() are an interface Shading owns, openrender::shading::IIlluminationHook
 * (illuminationHook.h), installed on a context with setIlluminationHook(). A context with a recording hook and services
 * whose hasIlluminationHook() is true runs four lights, one construct each, under both backends:
 *   illuminate(P)               illuminateBegin(P, null, null), illuminateEnd()
 *   illuminate(P, N, angle)     illuminateBegin(P, axis, angle), illuminateEnd()
 *   solar(axis, angle)          solarBegin(axis, angle), solarEnd()
 *   solar()                     solarBegin(null, null), solarEnd()
 * with the operands the shader holds (from (0.5 1.5 2.5), axis (0 0 1), angle 0.5), compared by value and by null-ness,
 * also when every point is inactive. With no hook installed the same light runs and nothing is called; with a hook
 * installed but the services answering false, nothing is called either (the hook is consulted only while
 * rendererIlluminationHook() is true). The existing tests of the hooks (tests/shading_parity/test_illumination_hook*.cpp) keep
 * their assertions; this one is the standalone check of the interface itself.
 *
 * Red on the starting tree: a build failure (there is no illuminationHook.h and no setIlluminationHook()).
 */
#include "cases_029.h"

#include "case_common.h"
#include "common/memory.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "libshader/shading/illuminationHook.h"

using namespace openrender::shading;

namespace {

    enum EHook { HOOK_ILLUMINATE_BEGIN, HOOK_ILLUMINATE_END, HOOK_SOLAR_BEGIN, HOOK_SOLAR_END };

    struct SHookCall {
            EHook hook;
            bool hasP = false, hasAxis = false, hasAngle = false;
            float P[3] = {0, 0, 0};
            float axis[3] = {0, 0, 0};
            float angle = 0;
    };

    bool sameVec(const float *a, const float *b) {
        for (int i = 0; i < 3; i++)
            if (std::fabs(a[i] - b[i]) > 1e-4f)
                return false;
        return true;
    }

    bool sameCall(const SHookCall &a, const SHookCall &b) {
        if (a.hook != b.hook || a.hasP != b.hasP || a.hasAxis != b.hasAxis || a.hasAngle != b.hasAngle)
            return false;
        if (a.hasP && !sameVec(a.P, b.P))
            return false;
        if (a.hasAxis && !sameVec(a.axis, b.axis))
            return false;
        if (a.hasAngle && std::fabs(a.angle - b.angle) > 1e-4f)
            return false;
        return true;
    }

    bool sameCalls(const std::vector<SHookCall> &a, const std::vector<SHookCall> &b) {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); i++)
            if (!sameCall(a[i], b[i]))
                return false;
        return true;
    }

    // The recording hook
    class CRecordingHook final : public IIlluminationHook {
        public:
            std::vector<SHookCall> calls;

            void solarBegin(const float *axis, const float *angle) override { calls.push_back(make(HOOK_SOLAR_BEGIN, nullptr, axis, angle)); }
            void solarEnd() override { calls.push_back(make(HOOK_SOLAR_END, nullptr, nullptr, nullptr)); }
            void illuminateBegin(const float *P, const float *axis, const float *angle) override { calls.push_back(make(HOOK_ILLUMINATE_BEGIN, P, axis, angle)); }
            void illuminateEnd() override { calls.push_back(make(HOOK_ILLUMINATE_END, nullptr, nullptr, nullptr)); }

            static SHookCall make(EHook h, const float *P, const float *axis, const float *angle) {
                SHookCall c;
                c.hook = h;
                if (P != nullptr) {
                    c.hasP = true;
                    memcpy(c.P, P, 3 * sizeof(float));
                }
                if (axis != nullptr) {
                    c.hasAxis = true;
                    memcpy(c.axis, axis, 3 * sizeof(float));
                }
                if (angle != nullptr) {
                    c.hasAngle = true;
                    c.angle = angle[0];
                }
                return c;
            }
    };

    const int kNumPoints = 4;

    // Executes a light once on a fresh state of four points (the last one back-facing for illuminate(), as the existing hook
    // tests have it); the calls the hook saw are in hook.calls afterwards
    void runLight(CHarness &h, CRecordingHook &hook, CProgrammableShaderInstance *light, bool allInactive) {
        static const float Ps[kNumPoints][3] = {{-1.5f, 0, 0}, {-0.5f, 0, 0}, {0.5f, 0, 0}, {1.5f, 0, 3}};
        static float costheta[kNumPoints] = {0, 0, 0, 0};
        CShadingState *ss = h.state();
        ss->numVertices = kNumPoints;
        ss->numRealVertices = kNumPoints;
        ss->shadingDim = SHADING_0D;
        ss->lights = nullptr;
        ss->freeLights = nullptr;
        ss->alights = nullptr;
        ss->costheta = costheta;
        for (int i = 0; i < kNumPoints; i++) {
            memcpy(ss->varying[VARIABLE_PS] + 3 * i, Ps[i], 3 * sizeof(float));
            initv(ss->Ns + 3 * i, 0, 0, 1);
            initv(ss->varying[VARIABLE_L] + 3 * i, 777, 777, 777);
            initv(ss->varying[VARIABLE_CL] + 3 * i, 0, 0, 0);
            ss->tags[i] = allInactive ? 1 : 0;
        }
        ss->numActive = allInactive ? 0 : kNumPoints;
        ss->numPassive = allInactive ? kNumPoints : 0;
        hook.calls.clear();

        CMemPage *localMemory = NULL;
        memoryInit(localMemory);
        float **locals = light->prepare(localMemory, ss->varying, ss->numVertices);
        h.context->execute(light, locals);
        memoryTini(localMemory);
    }

    struct SFixture {
            const char *shader;
            std::vector<SHookCall> expected;
    };

} // namespace

int caseIlluminationHookInterface(bool jit) {
    // The operands the lights hold: from (0.5 1.5 2.5), axis (0 0 1), angle 0.5
    const float from[3] = {0.5f, 1.5f, 2.5f};
    const float axis[3] = {0, 0, 1};
    const float angle = 0.5f;
    const std::vector<SFixture> fixtures = {
        {"cm_hook_illuminate1", {CRecordingHook::make(HOOK_ILLUMINATE_BEGIN, from, nullptr, nullptr), CRecordingHook::make(HOOK_ILLUMINATE_END, nullptr, nullptr, nullptr)}},
        {"cm_hook_illuminate3", {CRecordingHook::make(HOOK_ILLUMINATE_BEGIN, from, axis, &angle), CRecordingHook::make(HOOK_ILLUMINATE_END, nullptr, nullptr, nullptr)}},
        {"cm_hook_solar2", {CRecordingHook::make(HOOK_SOLAR_BEGIN, nullptr, axis, &angle), CRecordingHook::make(HOOK_SOLAR_END, nullptr, nullptr, nullptr)}},
        {"cm_hook_solar0", {CRecordingHook::make(HOOK_SOLAR_BEGIN, nullptr, nullptr, nullptr), CRecordingHook::make(HOOK_SOLAR_END, nullptr, nullptr, nullptr)}},
    };

    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, kNumPoints);
        CRecordingHook hook;

        CT_EXPECT(h.context->illuminationHook() == nullptr); // nothing installed on a new context
        h.context->setIlluminationHook(&hook);
        CT_EXPECT(h.context->illuminationHook() == &hook);

        for (const SFixture &fx : fixtures) {
            printf("  %s (%s)\n", fx.shader, jit ? "slo" : "rslo");
            CProgrammableShaderInstance *light = h.load(fx.shader, jit);
            CT_EXPECT(light != nullptr);
            if (light == nullptr)
                continue;

            // Hook installed and the services say it is on: the interface receives the calls, all points active or none
            svc.illuminationHookValue = true;
            for (int allInactive = 0; allInactive <= 1; allInactive++) {
                runLight(h, hook, light, allInactive != 0);
                if (!sameCalls(hook.calls, fx.expected))
                    fprintf(stderr, "    %s: %zu call(s), expected %zu (all %s)\n", fx.shader, hook.calls.size(), fx.expected.size(), allInactive ? "inactive" : "active");
                CT_EXPECT(sameCalls(hook.calls, fx.expected));
            }

            // Hook installed but the services answer false: the hook is not consulted
            svc.illuminationHookValue = false;
            runLight(h, hook, light, false);
            CT_EXPECT(hook.calls.empty());

            // No hook installed while the services say it is on: the light runs and nothing is called
            svc.illuminationHookValue = true;
            h.context->setIlluminationHook(nullptr);
            CT_EXPECT(h.context->illuminationHook() == nullptr);
            runLight(h, hook, light, false);
            CT_EXPECT(hook.calls.empty());
            h.context->setIlluminationHook(&hook);
        }
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
