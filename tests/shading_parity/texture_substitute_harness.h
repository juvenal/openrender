/**
 * tests/shading_parity/texture_substitute_harness.h
 *
 * Shared machinery of the substitute-texture-system tests (spec 027, T026 and T027): compile a
 * probe shader (tests/shading_parity/texture_substitute_<probe>.sl) as .rslo and as .slo under a
 * backend-specific surface name, load it through the real getShader() path, and run it once on a
 * four-point shading state with a CShadingContext whose rendering loop is empty (the pattern of
 * test_false_if_constructs.cpp). The results are the Ci values the probe wrote.
 *
 * The state is a SHADING_0D state: four points, du = dv = 0, so the interpreter's derivative
 * footprint collapses to the point itself (every corner equal) and the recorded requests are
 * independent of any grid. One RiBegin/RiEnd session per process.
 */

#ifndef TESTS_SHADING_PARITY_TEXTURE_SUBSTITUTE_HARNESS_H
#define TESTS_SHADING_PARITY_TEXTURE_SUBSTITUTE_HARNESS_H

#include <cassert>
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

// True when a and b agree to a few ulps of float
static inline bool nearlyEqual(float a, float b) {
    return std::fabs(a - b) <= 1e-6f * (1.0f + std::fabs(a) + std::fabs(b));
}

class CProbeContext : public CShadingContext {
    public:
        explicit CProbeContext(int thread) : CShadingContext(thread) {}
        void renderingLoop() override {}
};

// CSurface is abstract; instantiate() is never called here
class CProbeSurface : public CSurface {
    public:
        CProbeSurface(CAttributes *a, CXform *x) : CSurface(a, x) {}
        ~CProbeSurface() {}
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(false); }
};

static const int kNumPoints = 4;

// The inputs of the four points
static const float kS[kNumPoints] = {0.10f, 0.30f, 0.50f, 0.70f};
static const float kT[kNumPoints] = {0.20f, 0.40f, 0.60f, 0.80f};
static const float kN[kNumPoints][3] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {0.6f, 0, 0.8f}};
static const float kP[kNumPoints][3] = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}, {1.0f, 1.1f, 1.2f}};

static void setupState(CProbeContext *probe, CSurface *object) {
    CShadingState *ss = probe->currentShadingState;

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
        memcpy(ss->varying[VARIABLE_P] + 3 * i, kP[i], 3 * sizeof(float));
        memcpy(ss->varying[VARIABLE_PS] + 3 * i, kP[i], 3 * sizeof(float));
        memcpy(ss->varying[VARIABLE_N] + 3 * i, kN[i], 3 * sizeof(float));
        memcpy(ss->varying[VARIABLE_NG] + 3 * i, kN[i], 3 * sizeof(float));
        initv(ss->varying[VARIABLE_I] + 3 * i, 0, 0, 1);
        initv(ss->varying[VARIABLE_L] + 3 * i, 0, 0, 0);
        initv(ss->varying[VARIABLE_CI] + 3 * i, -1, -1, -1); // a sentinel: a probe that writes nothing is visible
        ss->varying[VARIABLE_S][i] = kS[i];
        ss->varying[VARIABLE_T][i] = kT[i];
        ss->varying[VARIABLE_DU][i] = 0;
        ss->varying[VARIABLE_DV][i] = 0;
        ss->varying[VARIABLE_TIME][i] = 0;
        ss->tags[i] = 0;
    }
    ss->numActive = kNumPoints;
    ss->numPassive = 0;
}

// The Ci each point ended with
struct SRunResult {
        float ci[kNumPoints][3];
};

static SRunResult runSurface(CProbeContext *probe, CProgrammableShaderInstance *surface, CSurface *object) {
    setupState(probe, object);
    CShadingState *ss = probe->currentShadingState;

    CMemPage *localMemory = NULL;
    memoryInit(localMemory);
    float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);
    probe->execute(surface, locals);

    SRunResult r;
    for (int i = 0; i < kNumPoints; i++)
        memcpy(r.ci[i], ss->varying[VARIABLE_CI] + 3 * i, 3 * sizeof(float));

    ss->lights = nullptr;
    ss->freeLights = nullptr;
    memoryTini(localMemory);
    return r;
}

static std::string readTextFile(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return {};
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        s.append(buf, n);
    fclose(f);
    return s;
}

static bool writeTextFile(const std::string &path, const std::string &text) {
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

// Compiles tests/shading_parity/texture_substitute_<probe>.sl as `<name>_<backend>` for "rslo" or "slo"
// into `dir` and loads it. The probe source names its surface tsub_<probe>; the copy is renamed.
static CProgrammableShaderInstance *compileAndLoad(const char *oshaderBin, const std::string &sourceDir, const std::string &dir,
                                                   const std::string &probe, const std::string &backend) {
    const std::string original = "tsub_" + probe;
    const std::string renamed = original + "_" + backend;
    std::string text = readTextFile(sourceDir + "/texture_substitute_" + probe + ".sl");
    EXPECT_TRUE(!text.empty());
    if (text.empty())
        return nullptr;
    for (size_t at = text.find(original); at != std::string::npos; at = text.find(original, at + renamed.size()))
        text.replace(at, original.size(), renamed);
    EXPECT_TRUE(writeTextFile(dir + "/" + renamed + ".sl", text));
    const bool jit = (backend == "slo");
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + renamed + ".sl", dir + "/" + renamed + (jit ? ".slo" : ".rslo"), jit));

    CShaderInstance *instance = CRenderer::context->getShader(renamed.c_str(), SL_SURFACE, 0, NULL, NULL);
    EXPECT_TRUE(instance != nullptr);
    CProgrammableShaderInstance *shader = dynamic_cast<CProgrammableShaderInstance *>(instance);
    EXPECT_TRUE(shader != nullptr);
    return shader;
}

#endif // TESTS_SHADING_PARITY_TEXTURE_SUBSTITUTE_HARNESS_H
