/**
 * tests/shading_parity/test_false_if_constructs.cpp
 *
 * Surface-shader constructs with side effects at block entry, inside an if
 * that is false for every point (GitHub #71, #96), as .rslo and as .slo. The
 * interpreter jumps over such a block, so neither backend may run any of it.
 *
 *   - "illuminance": illuminance() runs the light shaders. A light whose
 *     illuminate() calls the illumination hook makes a run visible. Inside
 *     "if (xcomp(P) > 10)" no light may run; inside "if (xcomp(P) > -10)"
 *     (the control) the light runs once.
 *   - "gather": gatherHeader builds a gather bundle and the gather loop
 *     iterates over its samples, reading the sample count from a temporary
 *     the false block never wrote (the #96 hang). currentShadingState->
 *     currentGather is set to a sentinel first: it is overwritten as soon as a
 *     bundle is built, so it must still hold the sentinel afterwards.
 *
 * The surface's scratch memory is filled with ASan's 0xbe bytes before
 * prepare(), as in test_illumination_hook_conditions.cpp. The light is
 * compiled as .rslo for both rows, so only the surface's backend differs.
 *
 * Needs the LLVM JIT (the test is only registered when it is built).
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/memory.h"
#include "libshader/shading/shading.h"
#include "ri/core/shader.h"
#include "ri/geometry/object.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"
#include "ri/state/xform.h"

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

// Shading context whose rendering loop is empty and which counts the
// illuminateBegin/illuminateEnd hook calls
class CRunProbeContext : public CShadingContext {
    public:
        explicit CRunProbeContext(int thread) : CShadingContext(thread) {}

        void renderingLoop() override {}

        int numBegins = 0;
        int numEnds = 0;

    protected:
        void illuminateBegin(const float *, const float *, const float *) override { ++numBegins; }
        void illuminateEnd() override { ++numEnds; }
};

// CSurface is abstract; instantiate() is never called here
class CTestSurface : public CSurface {
    public:
        CTestSurface(CAttributes *a, CXform *x) : CSurface(a, x) {}
        ~CTestSurface() {}
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(false); }
};

// Four points, xcomp(P) from -1.5 to 1.5
static const int kNumPoints = 4;

static void setupState(CRunProbeContext *probe, CSurface *object) {
    CShadingState *ss = probe->currentShadingState;
    static const float P[kNumPoints][3] = {{-1.5f, 0, 0}, {-0.5f, 0, 0}, {0.5f, 0, 0}, {1.5f, 0, 0}};

    ss->numVertices = kNumPoints;
    ss->numRealVertices = kNumPoints;
    ss->shadingDim = SHADING_0D;
    ss->lights = nullptr;
    ss->freeLights = nullptr;
    ss->alights = nullptr;
    ss->currentLight = nullptr;
    ss->lightsExecuted = FALSE;
    ss->currentObject = object;

    for (int i = 0; i < kNumPoints; i++) {
        memcpy(ss->varying[VARIABLE_P] + 3 * i, P[i], 3 * sizeof(float));
        memcpy(ss->varying[VARIABLE_PS] + 3 * i, P[i], 3 * sizeof(float));
        initv(ss->varying[VARIABLE_N] + 3 * i, 0, 0, 1);
        initv(ss->varying[VARIABLE_NG] + 3 * i, 0, 0, 1);
        initv(ss->varying[VARIABLE_I] + 3 * i, 0, 0, 1);
        initv(ss->varying[VARIABLE_CI] + 3 * i, 0, 0, 0);
        ss->varying[VARIABLE_DU][i] = 0;
        ss->varying[VARIABLE_DV][i] = 0;
        ss->varying[VARIABLE_TIME][i] = 0;
        ss->tags[i] = 0;
    }
    ss->numActive = kNumPoints;
    ss->numPassive = 0;
}

struct SRun {
        int numBegins;
        int numEnds;
        bool gatherBuilt;
};

// Runs the surface once, hook on, with its locals allocated from a page that
// holds the poison byte
static SRun runSurface(CRunProbeContext *probe, CProgrammableShaderInstance *surface, CSurface *object) {
    const unsigned int savedFlags = CRenderer::hiderFlags;
    CRenderer::hiderFlags |= HIDER_ILLUMINATIONHOOK;

    setupState(probe, object);
    probe->numBegins = probe->numEnds = 0;

    CShadingState *ss = probe->currentShadingState;
    static char sentinelStorage;
    CGatherBundle *const sentinel = reinterpret_cast<CGatherBundle *>(&sentinelStorage);
    ss->currentGather = sentinel;

    CMemPage *localMemory = NULL;
    memoryInit(localMemory);
    memset(localMemory->base, kPoisonByte, localMemory->totalSize);
    float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);

    probe->execute(surface, locals);

    SRun run = {probe->numBegins, probe->numEnds, ss->currentGather != sentinel};
    ss->currentGather = nullptr;
    ss->lights = nullptr;
    ss->freeLights = nullptr;

    memoryTini(localMemory);
    CRenderer::hiderFlags = savedFlags;
    return run;
}

static bool writeSource(const std::string &path, const std::string &text) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fputs(text.c_str(), f);
    fclose(f);
    return true;
}

static bool runOshader(const char *oshaderBin, const std::string &src, const std::string &out, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") + "-o \"" + out + "\" \"" + src + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

static CProgrammableShaderInstance *loadShader(const std::string &name, int type) {
    CShaderInstance *instance = CRenderer::context->getShader(name.c_str(), type, 0, NULL, NULL);
    EXPECT_TRUE(instance != nullptr);
    CProgrammableShaderInstance *shader = dynamic_cast<CProgrammableShaderInstance *>(instance);
    EXPECT_TRUE(shader != nullptr);
    return shader;
}

struct SFixture {
        const char *name;
        const char *body; // surface body
        SRun expected;
};

static void testFixture(CRunProbeContext *probe, const char *oshaderBin, const std::string &dir, CSurface *object, const SFixture &fx) {
    const std::string rsloName = std::string("fic_") + fx.name + "_rslo";
    const std::string sloName = std::string("fic_") + fx.name + "_slo";
    for (const std::string &name : {rsloName, sloName}) {
        const std::string text = "surface " + name + "(float ns = 4;)\n{\n    " + fx.body + "\n}\n";
        EXPECT_TRUE(writeSource(dir + "/" + name + ".sl", text));
    }
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + rsloName + ".sl", dir + "/" + rsloName + ".rslo", false));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + sloName + ".sl", dir + "/" + sloName + ".slo", true));

    CProgrammableShaderInstance *rslo = loadShader(rsloName, SL_SURFACE);
    CProgrammableShaderInstance *slo = loadShader(sloName, SL_SURFACE);
    if (!rslo || !slo)
        return;

    const SRun runs[2] = {runSurface(probe, rslo, object), runSurface(probe, slo, object)};
    const char *labels[2] = {".rslo", ".slo"};
    for (int k = 0; k < 2; k++) {
        const SRun &r = runs[k];
        printf("%s, %s\n", fx.name, labels[k]);
        fflush(stdout);
        const bool same = r.numBegins == fx.expected.numBegins && r.numEnds == fx.expected.numEnds &&
                          r.gatherBuilt == fx.expected.gatherBuilt;
        if (!same)
            fprintf(stderr, "    %s: %d illuminateBegin, %d illuminateEnd, gather bundle %s (expected %d, %d, %s)\n",
                    labels[k], r.numBegins, r.numEnds, r.gatherBuilt ? "built" : "not built",
                    fx.expected.numBegins, fx.expected.numEnds, fx.expected.gatherBuilt ? "built" : "not built");
        EXPECT_TRUE(same);
    }
}

int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "illuminance";
    if (mode != "illuminance" && mode != "gather") {
        fprintf(stderr, "usage: %s [illuminance|gather]\n", argv[0]);
        return 2;
    }

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_false_if_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    // The light: its illuminate() calls the hook whenever it runs
    const std::string lightName = "fic_light";
    EXPECT_TRUE(writeSource(dir + "/" + lightName + ".sl",
                            "light " + lightName + "()\n{\n    illuminate(point(0, 0, -5)) { Cl = color(1, 1, 1); }\n}\n"));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + lightName + ".sl", dir + "/" + lightName + ".rslo", false));

    std::vector<SFixture> fixtures;
    if (mode == "illuminance") {
        fixtures.push_back({"illuminance_false",
                            "color c = 0; if (xcomp(P) > 10) { illuminance(P, N, PI) { c += Cl; } } Ci = c;",
                            {0, 0, false}});
        fixtures.push_back({"illuminance_true",
                            "color c = 0; if (xcomp(P) > -10) { illuminance(P, N, PI) { c += Cl; } } Ci = c;",
                            {1, 1, false}});
    }
    else {
        fixtures.push_back({"gather_false",
                            "color hitc = 0, acc = 0; if (xcomp(P) > 10) { gather(\"illuminance\", P, N, PI / 2, ns, \"surface:Ci\", hitc) "
                            "{ acc += hitc; } else { acc += 1; } } Ci = acc;",
                            {0, 0, false}});
    }

    // cwd is searched first for shaders, so the freshly compiled fixtures load
    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    // One RiBegin/RiEnd session in the whole process. The probe is built after
    // RiWorldBegin because its constructor checkpoints globalArena.
    RiBegin(RI_NULL);
    RiWorldBegin();
    {
        CProgrammableShaderInstance *light = loadShader(lightName, SL_LIGHTSOURCE);
        CAttributes *attrs = new CAttributes();
        if (light)
            attrs->addLight(light);
        CXform *xform = new CXform();
        CTestSurface *object = new CTestSurface(attrs, xform);

        CRunProbeContext *probe = new CRunProbeContext(0);
        probe->updateState();

        for (const SFixture &fx : fixtures)
            testFixture(probe, oshaderBin, dir, object, fx);

        delete probe;
    }
    RiWorldEnd();
    RiEnd();

    if (chdir(savedCwd) != 0)
        perror("chdir (restoring working directory)");

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
