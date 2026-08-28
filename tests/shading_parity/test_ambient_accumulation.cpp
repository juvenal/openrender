/**
 * tests/shading_parity/test_ambient_accumulation.cpp
 *
 * Ambient-Cl accumulation regression tests (spec 014-jit-shading-parity,
 * T019/T020, User Story 3).
 *
 * T019: RSLShading::shade() (the public interpreter entry point,
 * src/libshader/shading/RSLShading.cpp:37-42) must not crash when it
 * reaches execute()'s execEnd: block for an ambient lightsource shader
 * with currentShadingState->alights == nullptr. Pre-fix, execute.cpp's
 * execEnd: block unconditionally dereferenced *alights (unlike its
 * already-guarded JIT-mirror block a few hundred lines earlier) --
 * a real crash reachable through this exact public entry point, since
 * RSLShading::shade() bypasses the normal prepareAmbient()/callAmbient()
 * allocator call sites that would otherwise guarantee alights is non-null.
 *
 * T020: the real CShadingContext::prepareAmbient() call site
 * (shading.cpp:1711-1749) must accumulate a single ambient light's Cl
 * exactly once into ss->alights->savedState[1], not twice. Pre-fix,
 * prepareAmbient() called light->illuminate() (which itself triggers
 * execute()'s own execEnd: accumulation) and then manually re-added
 * varying[VARIABLE_CL] into the same buffer a second time.
 *
 * Both tests use a minimal ambient light fixture (Cl = intensity; L = 0;
 * with no illuminate()/solar() block, matching shaders/ambientlight.sl's
 * shape) compiled ONLY to .rslo -- this exercises the interpreter
 * backend exclusively, which is what both bugs live in.
 */

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "libshader/include/openrender/RSLShading.h"
#include "ri/attributes.h"
#include "ri/memory.h"
#include "ri/object.h"
#include "ri/renderer.h"
#include "ri/rendererContext.h"
#include "ri/ri.h"
#include "ri/shader.h"
#include "ri/shading.h"
#include "ri/xform.h"

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr) do { \
    if (expr) { ++g_passed; } \
    else { fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); ++g_failed; } \
} while (0)

// Minimal concrete CSurface subclass -- CSurface itself is abstract
// (CObject::instantiate() is pure virtual and CSurface never overrides
// it). Mirrors the established CPhonySurface precedent in
// src/ri/photon.cpp:49-55: instantiate() is never actually called in
// this test (attributes/lights are constructed manually, not lazily
// instantiated via the renderer's object-instantiation machinery), so
// an assert(false) stub is sufficient.
class CTestSurface : public CSurface {
    public:
        CTestSurface(CAttributes *a, CXform *x) : CSurface(a, x) {}
        ~CTestSurface() {}
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(false); }
};

static bool writeFixture(const std::string &path, const std::string &shaderKind,
                          const std::string &shaderName, const std::string &paramDecl,
                          const std::string &body) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "%s %s(\n%s\n) {\n%s}\n",
            shaderKind.c_str(), shaderName.c_str(), paramDecl.c_str(), body.c_str());
    fclose(f);
    return true;
}

// Invoke the real oshader CLI (built by this project) to compile srcPath to
// outPath as .rslo (interpreter bytecode only -- neither T019 nor T020
// touches the JIT backend). Returns true on a clean (exit 0) run.
static bool runOshaderRslo(const char *oshaderBin, const std::string &srcPath,
                            const std::string &outPath) {
    std::string cmd = std::string("\"") + oshaderBin + "\" -o \"" + outPath + "\" \"" +
                       srcPath + "\" >/dev/null 2>&1";
    int rc = system(cmd.c_str());
    return rc == 0;
}

// Compiles a minimal ambient light fixture (no illuminate()/solar() block,
// so PARAMETER_NONAMBIENT/SHADERFLAGS_NONAMBIENT stay unset) to .rslo under
// a fresh temp dir and loads it through the real getShader() runtime path.
// Leaves cwd changed to the temp dir on success (matching the surrounding
// RiBegin/RiWorldBegin window); caller restores cwd after RiEnd().
static CProgrammableShaderInstance *loadAmbientFixture(const char *oshaderBin,
                                                         const char *label,
                                                         std::string &savedCwdOut) {
    char tmplBuf[] = "/tmp/shading_parity_ambient_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir) return nullptr;

    const std::string dir = tmpDir;
    const std::string name = std::string("ambient_") + label;
    const std::string src = dir + "/" + name + ".sl";
    const std::string out = dir + "/" + name + ".rslo";

    EXPECT_TRUE(writeFixture(src, "light", name,
                              "    float intensity = 1.0",
                              "    Cl = intensity;\n    L = 0;\n"));
    EXPECT_TRUE(runOshaderRslo(oshaderBin, src, out));

    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    savedCwdOut = savedCwd;
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    CShaderInstance *instance =
        CRenderer::context->getShader(name.c_str(), SL_LIGHTSOURCE, 0, NULL, NULL);
    EXPECT_TRUE(instance != nullptr);
    if (!instance) return nullptr;

    CProgrammableShaderInstance *light = dynamic_cast<CProgrammableShaderInstance *>(instance);
    EXPECT_TRUE(light != nullptr);
    return light;
}

// T019: RSLShading::shade() must not crash when currentShadingState->alights
// is nullptr for an ambient lightsource shader (SC-004).
static void test_t019_no_crash_when_alights_null() {
    printf("T019: RSLShading::shade() does not crash when alights == nullptr\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin) return;

    RiBegin(RI_NULL);
    std::string savedCwd;
    CProgrammableShaderInstance *light = loadAmbientFixture(oshaderBin, "t019", savedCwd);

    if (light) {
        RiWorldBegin();

        CShadingContext *ctx = CRenderer::contexts[0];
        EXPECT_TRUE(ctx != nullptr);

        if (ctx) {
            // updateState() fully drains the free-state list before calling
            // newState(), guaranteeing the fresh-allocation branch runs --
            // which never touches alights, so it is deterministically
            // nullptr here, reproducing the pre-fix crash precondition.
            ctx->updateState();
            CShadingState *ss = ctx->currentShadingState;
            EXPECT_TRUE(ss != nullptr);

            if (ss) {
                ss->numVertices = 1;
                ss->numActive = 1;
                ss->numPassive = 0;
                ss->tags[0] = 0;
                ss->alights = nullptr;

                CAttributes *attrs = new CAttributes();
                CXform *xform = new CXform();
                ss->currentObject = new CTestSurface(attrs, xform);

                CMemPage *localMemory = NULL;
                memoryInit(localMemory);
                float **locals = light->prepare(localMemory, ss->varying, ss->numVertices);

                RSLShadingState state;
                state.numVertices = ss->numVertices;
                state.varying = ss->varying;
                state.locals = locals;
                state.tags = ss->tags;
                state.ctx = ctx;
                state.lighting = nullptr;
                state.texture = nullptr;
                state.trace = nullptr;

                // The pass condition IS reaching this line: pre-fix, execute()'s
                // execEnd: block unconditionally dereferenced *alights and
                // crashed before returning to this test function.
                RSLShading::shade(light, state);
                EXPECT_TRUE(true);

                memoryTini(localMemory);
            }
        }

        RiWorldEnd();
    }

    CShaderInstance *toDetach = light;
    if (toDetach) toDetach->detach();
    RiEnd();

    if (!savedCwd.empty()) chdir(savedCwd.c_str());
}

// T020: prepareAmbient() must accumulate a single ambient light's Cl exactly
// once, not twice (the double-count regression T022 fixes).
static void test_t020_ambient_accumulates_once() {
    printf("T020: prepareAmbient() accumulates ambient Cl exactly once (not twice)\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin) return;

    RiBegin(RI_NULL);
    std::string savedCwd;
    CProgrammableShaderInstance *light = loadAmbientFixture(oshaderBin, "t020", savedCwd);

    if (light) {
        RiWorldBegin();

        CShadingContext *ctx = CRenderer::contexts[0];
        EXPECT_TRUE(ctx != nullptr);

        if (ctx) {
            CAttributes *attrs = new CAttributes();
            attrs->addLight(light);

            CXform *xform = new CXform();
            CTestSurface *surface = new CTestSurface(attrs, xform);

            ctx->updateState();
            CShadingState *ss = ctx->currentShadingState;
            EXPECT_TRUE(ss != nullptr);

            if (ss) {
                ss->numVertices = 1;
                ss->numActive = 1;
                ss->numPassive = 0;
                ss->tags[0] = 0;
                ss->ambientLightsExecuted = 0;
                ss->currentObject = surface;

                // The real public call site (shading.cpp:1711-1749) -- drives
                // light->prepare()/illuminate() for every non-ambient-excluded
                // light on the object's attributes and accumulates Cl into
                // ss->alights->savedState[1].
                ctx->prepareAmbient();

                EXPECT_TRUE(ss->alights != nullptr);
                if (ss->alights != nullptr) {
                    const float *Cl = ss->alights->savedState[1];
                    printf("  savedState[1] (Cl) = (%f, %f, %f)\n", Cl[0], Cl[1], Cl[2]);
                    // One light, intensity=1: exactly one accumulation gives
                    // (1,1,1). Pre-T022, the manual re-accumulation loop
                    // double-counted, giving (2,2,2).
                    EXPECT_TRUE(fabsf(Cl[0] - 1.0f) < 0.001f);
                    EXPECT_TRUE(fabsf(Cl[1] - 1.0f) < 0.001f);
                    EXPECT_TRUE(fabsf(Cl[2] - 1.0f) < 0.001f);
                }
            }
        }

        RiWorldEnd();
    }

    CShaderInstance *toDetach = light;
    if (toDetach) toDetach->detach();
    RiEnd();

    if (!savedCwd.empty()) chdir(savedCwd.c_str());
}

int main() {
    test_t019_no_crash_when_alights_null();
    test_t020_ambient_accumulates_once();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
