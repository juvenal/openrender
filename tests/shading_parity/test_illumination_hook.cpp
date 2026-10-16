/**
 * tests/shading_parity/test_illumination_hook.cpp
 *
 * The illumination hook under both shader backends (spec
 * 024-shading-services-routing, T012; research R2;
 * contracts/illumination-hook.md).
 *
 * A hider that sets HIDER_ILLUMINATIONHOOK (the photon hider) wants each
 * illuminate()/solar() construct handed to it through four virtual hooks on
 * CShadingContext. The interpreter always did that. The JIT did not: it never
 * consulted the flag, and for solar() with no arguments it emitted no begin
 * call at all.
 *
 * This test runs four light shaders (illuminate(P), illuminate(P, N, a),
 * solar(D, a) and solar()), each compiled to .rslo and to .slo, through
 * CShadingContext::execute() on a probe context whose hooks only log their
 * arguments. It checks:
 *
 *   - hook on: the call log equals the contract table and is the same for
 *     both backends; the construct leaves the tags, the counts and L alone;
 *     the hook is still called when every point is inactive;
 *   - hook off: no hook calls; L, the tags and the counts are equal across
 *     the backends, and solar() leaves L == Ps for the active points;
 *   - toggle: on one loaded .slo, a hook-on run followed by a hook-off run
 *     gives the hook-off result (compiled code does not cache the flag).
 *
 * Arguments are compared by value and by null-ness, never by pointer: the
 * interpreter passes its operand pointer and the JIT a pointer into its own
 * variable storage.
 *
 * Costheta is set for every point so that, before the fix, the JIT rows fail
 * by assertion rather than by crashing; the photon hider leaves it unset,
 * which is the crash measured in research R4.
 *
 * Needs the LLVM JIT (the test is only registered when it is built).
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/memory.h"
#include "libshader/shading/shading.h"
#include "ri/core/shader.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (expr) {                                                            \
            ++g_passed;                                                        \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

// One logged hook call. P, N and theta are captured by value; has* records
// whether the hider was handed a null pointer.
enum EHook { HOOK_ILLUMINATE_BEGIN,
             HOOK_ILLUMINATE_END,
             HOOK_SOLAR_BEGIN,
             HOOK_SOLAR_END };

struct SHookCall {
        EHook hook;
        bool hasP = false, hasN = false, hasTheta = false;
        float P[3] = {0, 0, 0};
        float N[3] = {0, 0, 0};
        float theta = 0;
};

static const char *hookName(EHook h) {
    switch (h) {
        case HOOK_ILLUMINATE_BEGIN:
            return "illuminateBegin";
        case HOOK_ILLUMINATE_END:
            return "illuminateEnd";
        case HOOK_SOLAR_BEGIN:
            return "solarBegin";
        default:
            return "solarEnd";
    }
}

static bool sameVec(const float *a, const float *b) {
    for (int i = 0; i < 3; i++)
        if (fabsf(a[i] - b[i]) > 1e-4f)
            return false;
    return true;
}

static bool sameCall(const SHookCall &a, const SHookCall &b) {
    if (a.hook != b.hook || a.hasP != b.hasP || a.hasN != b.hasN || a.hasTheta != b.hasTheta)
        return false;
    if (a.hasP && !sameVec(a.P, b.P))
        return false;
    if (a.hasN && !sameVec(a.N, b.N))
        return false;
    if (a.hasTheta && fabsf(a.theta - b.theta) > 1e-4f)
        return false;
    return true;
}

static void printCalls(const char *label, const std::vector<SHookCall> &calls) {
    fprintf(stderr, "    %s: %zu call(s)", label, calls.size());
    for (const SHookCall &c : calls)
        fprintf(stderr, " %s%s%s%s", hookName(c.hook), c.hasP ? " P" : "", c.hasN ? " N" : "", c.hasTheta ? " theta" : "");
    fprintf(stderr, "\n");
}

static bool sameCalls(const std::vector<SHookCall> &a, const std::vector<SHookCall> &b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++)
        if (!sameCall(a[i], b[i]))
            return false;
    return true;
}

// Shading context whose rendering loop is empty and whose four hooks only log
class CHookProbeContext : public CShadingContext {
    public:
        explicit CHookProbeContext(int thread) : CShadingContext(thread) {}

        void renderingLoop() override {}

        std::vector<SHookCall> calls;

    protected:
        void illuminateBegin(const float *P, const float *N, const float *theta) override {
            SHookCall c;
            c.hook = HOOK_ILLUMINATE_BEGIN;
            capture(c, P, N, theta);
            calls.push_back(c);
        }
        void illuminateEnd() override {
            SHookCall c;
            c.hook = HOOK_ILLUMINATE_END;
            calls.push_back(c);
        }
        void solarBegin(const float *N, const float *theta) override {
            SHookCall c;
            c.hook = HOOK_SOLAR_BEGIN;
            capture(c, nullptr, N, theta);
            calls.push_back(c);
        }
        void solarEnd() override {
            SHookCall c;
            c.hook = HOOK_SOLAR_END;
            calls.push_back(c);
        }

    private:
        static void capture(SHookCall &c, const float *P, const float *N, const float *theta) {
            if (P) {
                c.hasP = true;
                memcpy(c.P, P, 3 * sizeof(float));
            }
            if (N) {
                c.hasN = true;
                memcpy(c.N, N, 3 * sizeof(float));
            }
            if (theta) {
                c.hasTheta = true;
                c.theta = theta[0];
            }
        }
};

// The shading state the fixtures run on: four points, the last one back-facing
// for illuminate(), so that the hook-off runs have a passive point to compare.
static const int kNumPoints = 4;
static const float kSentinelL = 777.0f;

struct SRunResult {
        std::vector<SHookCall> calls;
        std::vector<int> tags;
        std::vector<float> L;
        int numActive = 0, numPassive = 0;
};

static void setupState(CHookProbeContext *probe, bool allInactive) {
    CShadingState *ss = probe->currentShadingState;
    static const float Ps[kNumPoints][3] = {{-1.5f, 0, 0}, {-0.5f, 0, 0}, {0.5f, 0, 0}, {1.5f, 0, 3}};
    static float costheta[kNumPoints] = {0, 0, 0, 0};

    ss->numVertices = kNumPoints;
    ss->numRealVertices = kNumPoints;
    ss->lights = nullptr;
    ss->freeLights = nullptr;
    ss->alights = nullptr;
    ss->costheta = costheta;

    for (int i = 0; i < kNumPoints; i++) {
        memcpy(ss->varying[VARIABLE_PS] + 3 * i, Ps[i], 3 * sizeof(float));
        initv(ss->Ns + 3 * i, 0, 0, 1);
        initv(ss->varying[VARIABLE_L] + 3 * i, kSentinelL, kSentinelL, kSentinelL);
        initv(ss->varying[VARIABLE_CL] + 3 * i, 0, 0, 0);
        ss->tags[i] = allInactive ? 1 : 0;
    }
    ss->numActive = allInactive ? 0 : kNumPoints;
    ss->numPassive = allInactive ? kNumPoints : 0;
}

// Executes the light once on a fresh state and returns what it left behind
static SRunResult runLight(CHookProbeContext *probe, CProgrammableShaderInstance *light, bool hookOn, bool allInactive) {
    const unsigned int savedFlags = CRenderer::hiderFlags;
    if (hookOn)
        CRenderer::hiderFlags |= HIDER_ILLUMINATIONHOOK;
    else
        CRenderer::hiderFlags &= ~HIDER_ILLUMINATIONHOOK;

    setupState(probe, allInactive);
    probe->calls.clear();

    CShadingState *ss = probe->currentShadingState;
    CMemPage *localMemory = NULL;
    memoryInit(localMemory);
    float **locals = light->prepare(localMemory, ss->varying, ss->numVertices);

    probe->execute(light, locals);

    SRunResult r;
    r.calls = probe->calls;
    r.tags.assign(ss->tags, ss->tags + kNumPoints);
    r.L.assign(ss->varying[VARIABLE_L], ss->varying[VARIABLE_L] + 3 * kNumPoints);
    r.numActive = ss->numActive;
    r.numPassive = ss->numPassive;

    memoryTini(localMemory);
    CRenderer::hiderFlags = savedFlags;
    return r;
}

// What an untouched state looks like (hook on: the constructs touch nothing)
static void expectUntouched(const char *label, const SRunResult &r, bool allInactive) {
    bool ok = true;
    for (int i = 0; i < kNumPoints; i++)
        ok = ok && r.tags[i] == (allInactive ? 1 : 0);
    ok = ok && r.numActive == (allInactive ? 0 : kNumPoints) && r.numPassive == (allInactive ? kNumPoints : 0);
    for (float v : r.L)
        ok = ok && v == kSentinelL;
    if (!ok)
        fprintf(stderr, "    %s: tags/counts/L were modified (numActive=%d numPassive=%d)\n", label, r.numActive, r.numPassive);
    EXPECT_TRUE(ok);
}

static bool sameState(const SRunResult &a, const SRunResult &b) {
    if (a.tags != b.tags || a.numActive != b.numActive || a.numPassive != b.numPassive)
        return false;
    for (size_t i = 0; i < a.L.size(); i++)
        if (fabsf(a.L[i] - b.L[i]) > 1e-4f)
            return false;
    return true;
}

static SHookCall callOf(EHook h, const float *P = nullptr, const float *N = nullptr, const float *theta = nullptr) {
    SHookCall c;
    c.hook = h;
    if (P) {
        c.hasP = true;
        memcpy(c.P, P, 3 * sizeof(float));
    }
    if (N) {
        c.hasN = true;
        memcpy(c.N, N, 3 * sizeof(float));
    }
    if (theta) {
        c.hasTheta = true;
        c.theta = *theta;
    }
    return c;
}

struct SFixture {
        const char *name;
        const char *body;
        bool isSolar0;
        std::vector<SHookCall> expected; // hook active: the contract table
};

static bool writeSource(const std::string &path, const std::string &name, const std::string &body) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fprintf(f,
            "light %s(point from = point(0.5, 1.5, 2.5); vector axis = vector(0, 0, 1); float angle = 0.5;)\n"
            "{\n    %s\n}\n",
            name.c_str(), body.c_str());
    fclose(f);
    return true;
}

static bool runOshader(const char *oshaderBin, const std::string &src, const std::string &out, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") + "-o \"" + out + "\" \"" + src + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

static CProgrammableShaderInstance *loadLight(const std::string &name) {
    CShaderInstance *instance = CRenderer::context->getShader(name.c_str(), SL_LIGHTSOURCE, 0, NULL, NULL);
    EXPECT_TRUE(instance != nullptr);
    CProgrammableShaderInstance *light = dynamic_cast<CProgrammableShaderInstance *>(instance);
    EXPECT_TRUE(light != nullptr);
    return light;
}

static void testFixture(CHookProbeContext *probe, const char *oshaderBin, const std::string &dir, const SFixture &fx) {
    printf("%s\n", fx.name);

    const std::string rsloName = std::string("hk_") + fx.name + "_rslo";
    const std::string sloName = std::string("hk_") + fx.name + "_slo";
    EXPECT_TRUE(writeSource(dir + "/" + rsloName + ".sl", rsloName, fx.body));
    EXPECT_TRUE(writeSource(dir + "/" + sloName + ".sl", sloName, fx.body));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + rsloName + ".sl", dir + "/" + rsloName + ".rslo", false));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + sloName + ".sl", dir + "/" + sloName + ".slo", true));

    CProgrammableShaderInstance *rslo = loadLight(rsloName);
    CProgrammableShaderInstance *slo = loadLight(sloName);
    if (!rslo || !slo)
        return;

    // ---- Hook on: the contract table, identical for both backends
    for (int allInactive = 0; allInactive <= 1; allInactive++) {
        const char *mode = allInactive ? "all points inactive" : "all points active";
        const SRunResult onR = runLight(probe, rslo, true, allInactive);
        const SRunResult onJ = runLight(probe, slo, true, allInactive);

        printf("  hook on, %s, .rslo\n", mode);
        if (!sameCalls(onR.calls, fx.expected))
            printCalls(".rslo calls", onR.calls);
        EXPECT_TRUE(sameCalls(onR.calls, fx.expected));
        expectUntouched(".rslo", onR, allInactive);

        printf("  hook on, %s, .slo\n", mode);
        if (!sameCalls(onJ.calls, fx.expected))
            printCalls(".slo calls", onJ.calls);
        EXPECT_TRUE(sameCalls(onJ.calls, fx.expected));
        expectUntouched(".slo", onJ, allInactive);
    }

    // ---- Hook off: no calls; state equal across the backends
    printf("  hook off\n");
    const SRunResult offR = runLight(probe, rslo, false, false);
    const SRunResult offJ = runLight(probe, slo, false, false);
    EXPECT_TRUE(offR.calls.empty());
    EXPECT_TRUE(offJ.calls.empty());
    if (!sameState(offR, offJ)) {
        fprintf(stderr, "    state differs: .rslo numActive=%d numPassive=%d, .slo numActive=%d numPassive=%d\n", offR.numActive, offR.numPassive, offJ.numActive, offJ.numPassive);
        for (int i = 0; i < kNumPoints; i++)
            fprintf(stderr, "      point %d: .rslo tag=%d L=(%g %g %g)  .slo tag=%d L=(%g %g %g)\n", i, offR.tags[i], offR.L[3 * i], offR.L[3 * i + 1], offR.L[3 * i + 2], offJ.tags[i], offJ.L[3 * i], offJ.L[3 * i + 1], offJ.L[3 * i + 2]);
    }
    EXPECT_TRUE(sameState(offR, offJ));

    if (fx.isSolar0) {
        // solar() sets L = Ps for every active point
        static const float Ps[kNumPoints][3] = {{-1.5f, 0, 0}, {-0.5f, 0, 0}, {0.5f, 0, 0}, {1.5f, 0, 3}};
        bool rsloOk = true, sloOk = true;
        for (int i = 0; i < kNumPoints; i++) {
            rsloOk = rsloOk && sameVec(&offR.L[3 * i], Ps[i]);
            sloOk = sloOk && sameVec(&offJ.L[3 * i], Ps[i]);
        }
        EXPECT_TRUE(rsloOk);
        EXPECT_TRUE(sloOk);
    }

    // ---- Toggle: the same loaded .slo, hook on, then off
    printf("  toggle on the same loaded .slo\n");
    (void)runLight(probe, slo, true, false);
    const SRunResult again = runLight(probe, slo, false, false);
    EXPECT_TRUE(again.calls.empty());
    EXPECT_TRUE(sameState(again, offR));
}

int main() {
    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_hook_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    // The hider is handed what the shader's operands hold: from (0.5 1.5 2.5),
    // axis (0 0 1) and angle 0.5. solar(axis, angle) reaches solarBegin(axis,
    // angle); solar() reaches solarBegin(null, null).
    const float from[3] = {0.5f, 1.5f, 2.5f};
    const float axis[3] = {0, 0, 1};
    const float angle = 0.5f;

    std::vector<SFixture> fixtures;
    fixtures.push_back({"illuminate1", "illuminate(from) { Cl = color(1, 1, 1); }", false, {callOf(HOOK_ILLUMINATE_BEGIN, from), callOf(HOOK_ILLUMINATE_END)}});
    fixtures.push_back({"illuminate3", "illuminate(from, axis, angle) { Cl = color(1, 1, 1); }", false, {callOf(HOOK_ILLUMINATE_BEGIN, from, axis, &angle), callOf(HOOK_ILLUMINATE_END)}});
    fixtures.push_back({"solar2", "solar(axis, angle) { Cl = color(1, 1, 1); }", false, {callOf(HOOK_SOLAR_BEGIN, nullptr, axis, &angle), callOf(HOOK_SOLAR_END)}});
    fixtures.push_back({"solar0", "solar() { Cl = color(1, 1, 1); }", true, {callOf(HOOK_SOLAR_BEGIN), callOf(HOOK_SOLAR_END)}});

    // cwd is searched first for shaders, so the freshly compiled fixtures load
    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    // One RiBegin/RiEnd session in the whole process. The probe is built after
    // RiWorldBegin because its constructor checkpoints globalArena.
    RiBegin(RI_NULL);
    RiWorldBegin();
    {
        CHookProbeContext *probe = new CHookProbeContext(0);
        probe->updateState();

        for (const SFixture &fx : fixtures)
            testFixture(probe, oshaderBin, dir, fx);

        delete probe;
    }
    RiWorldEnd();
    RiEnd();

    if (chdir(savedCwd) != 0)
        perror("chdir (restoring working directory)");

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
