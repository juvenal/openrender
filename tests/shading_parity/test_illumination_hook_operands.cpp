/**
 * tests/shading_parity/test_illumination_hook_operands.cpp
 *
 * The values the illumination hook is handed, with lane 0 inactive
 * (GitHub #68).
 *
 * With the hook on, illuminate()/solar() pass the hider pointers to their
 * operands, and the hider reads the first element (contracts/
 * illumination-hook.md in spec 024). The compiler broadcasts every operand
 * into a varying temporary first ("vuvector temporary_0 from"), even when it
 * is a uniform shader parameter, and the broadcast is tag-masked: an inactive
 * lane keeps whatever the temporary held before. When point 0 is inactive the
 * hook therefore reads either scratch memory the shader never wrote or a
 * stale value from an earlier use of the same register.
 *
 * The JIT lifts a temporary written once to uniform and writes it whatever
 * the tags, so .slo hands over the right value for the first kind. A register
 * the compiler reuses (illuminate(from, axis, angle) inside the conditional
 * below reuses the one that held xcomp(Ps) for the angle) is written twice,
 * stays varying, and is stale under both backends.
 * ShadingParity_IlluminationHook's "all points inactive" .rslo rows hit the
 * first kind whenever the reused memory held something else (always under
 * ASan, which fills fresh allocations with 0xbe).
 *
 * This test makes the read deterministic: the scratch page the light's locals
 * are allocated from is filled with 0xbe bytes before prepare() (a float of
 * -0.372549), which is what ASan does. It runs illuminate(P),
 * illuminate(P, N, a) and solar(D, a), as .rslo and as .slo, in two shapes:
 *
 *   - top level, entered with every point already inactive (the state
 *     ShadingParity_IlluminationHook's "all points inactive" rows use);
 *   - inside "if (xcomp(Ps) > 0)", entered with every point active, so that
 *     points 0 and 1 are inactive and points 2 and 3 active in the construct.
 *     This is what a photon pass can reach: the photon hider runs lights with
 *     every point active, and a varying conditional in the light shader
 *     deactivates some of them.
 *
 * Each run must hand the hook the parameter values: from (0.5 1.5 2.5),
 * axis (0 0 1) and angle 0.5.
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

// The byte ASan fills fresh heap allocations with
static const unsigned char kPoisonByte = 0xbe;

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
        if (!(fabsf(a[i] - b[i]) <= 1e-4f))
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
    if (a.hasTheta && !(fabsf(a.theta - b.theta) <= 1e-4f))
        return false;
    return true;
}

static bool sameCalls(const std::vector<SHookCall> &a, const std::vector<SHookCall> &b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++)
        if (!sameCall(a[i], b[i]))
            return false;
    return true;
}

// Prints the values, so that a red row shows the poison it read
static void printCalls(const char *label, const std::vector<SHookCall> &calls) {
    fprintf(stderr, "    %s: %zu call(s)\n", label, calls.size());
    for (const SHookCall &c : calls) {
        fprintf(stderr, "      %s", hookName(c.hook));
        if (c.hasP)
            fprintf(stderr, " P=(%g %g %g)", c.P[0], c.P[1], c.P[2]);
        if (c.hasN)
            fprintf(stderr, " N=(%g %g %g)", c.N[0], c.N[1], c.N[2]);
        if (c.hasTheta)
            fprintf(stderr, " theta=%g", c.theta);
        fprintf(stderr, "\n");
    }
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

// Four points; points 0 and 1 have xcomp(Ps) < 0, points 2 and 3 > 0
static const int kNumPoints = 4;

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
        initv(ss->varying[VARIABLE_L] + 3 * i, 0, 0, 0);
        initv(ss->varying[VARIABLE_CL] + 3 * i, 0, 0, 0);
        ss->tags[i] = allInactive ? 1 : 0;
    }
    ss->numActive = allInactive ? 0 : kNumPoints;
    ss->numPassive = allInactive ? kNumPoints : 0;
}

// Runs the light once, hook on, with its locals allocated from a page that
// holds the poison byte, and returns the hook calls
static std::vector<SHookCall> runLight(CHookProbeContext *probe, CProgrammableShaderInstance *light, bool allInactive) {
    const unsigned int savedFlags = CRenderer::hiderFlags;
    CRenderer::hiderFlags |= HIDER_ILLUMINATIONHOOK;

    setupState(probe, allInactive);
    probe->calls.clear();

    CShadingState *ss = probe->currentShadingState;
    CMemPage *localMemory = NULL;
    memoryInit(localMemory);
    memset(localMemory->base, kPoisonByte, localMemory->totalSize);
    float **locals = light->prepare(localMemory, ss->varying, ss->numVertices);

    probe->execute(light, locals);
    std::vector<SHookCall> calls = probe->calls;

    memoryTini(localMemory);
    CRenderer::hiderFlags = savedFlags;
    return calls;
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
        bool allInactive; // entered with every point inactive
        std::vector<SHookCall> expected;
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
    const std::string rsloName = std::string("hko_") + fx.name + "_rslo";
    const std::string sloName = std::string("hko_") + fx.name + "_slo";
    EXPECT_TRUE(writeSource(dir + "/" + rsloName + ".sl", rsloName, fx.body));
    EXPECT_TRUE(writeSource(dir + "/" + sloName + ".sl", sloName, fx.body));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + rsloName + ".sl", dir + "/" + rsloName + ".rslo", false));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + sloName + ".sl", dir + "/" + sloName + ".slo", true));

    CProgrammableShaderInstance *rslo = loadLight(rsloName);
    CProgrammableShaderInstance *slo = loadLight(sloName);
    if (!rslo || !slo)
        return;

    const std::vector<SHookCall> r = runLight(probe, rslo, fx.allInactive);
    const std::vector<SHookCall> j = runLight(probe, slo, fx.allInactive);

    printf("%s, .rslo\n", fx.name);
    fflush(stdout);
    if (!sameCalls(r, fx.expected))
        printCalls(".rslo calls", r);
    EXPECT_TRUE(sameCalls(r, fx.expected));

    printf("%s, .slo\n", fx.name);
    fflush(stdout);
    if (!sameCalls(j, fx.expected))
        printCalls(".slo calls", j);
    EXPECT_TRUE(sameCalls(j, fx.expected));
}

int main() {
    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_hook_operands_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    const float from[3] = {0.5f, 1.5f, 2.5f};
    const float axis[3] = {0, 0, 1};
    const float angle = 0.5f;

    const std::vector<SHookCall> ill1 = {callOf(HOOK_ILLUMINATE_BEGIN, from), callOf(HOOK_ILLUMINATE_END)};
    const std::vector<SHookCall> ill3 = {callOf(HOOK_ILLUMINATE_BEGIN, from, axis, &angle), callOf(HOOK_ILLUMINATE_END)};
    const std::vector<SHookCall> sol2 = {callOf(HOOK_SOLAR_BEGIN, nullptr, axis, &angle), callOf(HOOK_SOLAR_END)};

    std::vector<SFixture> fixtures;
    fixtures.push_back({"illuminate1_inactive", "illuminate(from) { Cl = color(1, 1, 1); }", true, ill1});
    fixtures.push_back({"illuminate3_inactive", "illuminate(from, axis, angle) { Cl = color(1, 1, 1); }", true, ill3});
    fixtures.push_back({"solar2_inactive", "solar(axis, angle) { Cl = color(1, 1, 1); }", true, sol2});
    fixtures.push_back({"illuminate1_conditional", "if (xcomp(Ps) > 0) { illuminate(from) { Cl = color(1, 1, 1); } }", false, ill1});
    fixtures.push_back({"illuminate3_conditional", "if (xcomp(Ps) > 0) { illuminate(from, axis, angle) { Cl = color(1, 1, 1); } }", false, ill3});
    fixtures.push_back({"solar2_conditional", "if (xcomp(Ps) > 0) { solar(axis, angle) { Cl = color(1, 1, 1); } }", false, sol2});

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
