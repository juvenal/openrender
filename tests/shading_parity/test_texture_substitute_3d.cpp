/**
 * tests/shading_parity/test_texture_substitute_3d.cpp
 *
 * ShadingParity_TextureSubstitute3D (spec 027, T-S1 part two, T027, FR-012, FR-018(b), User Story 3).
 *
 * texture3d() and bake3d() reach their 3D texture only through the renderer services: getTexture3d()
 * hands out the object and the three forwarders (texture3dResolve/Lookup/Store, contracts/
 * renderer-services.md) carry the channel operations, so Shading never calls a CTexture3d method.
 * This test installs CTextureServices (recording_texture_system.h), whose getTexture3d() returns a
 * token and whose forwarders record their arguments, and runs the two probes as .rslo and as .slo:
 *
 *   - each backend resolves (at least once, with no user channels), then looks up (texture3d()) or
 *     stores (bake3d()) once per point, with the point and the normal of that point and the radius
 *     the lookup computes (0 here: du = dv = 0), and the forwarders receive the very token
 *     getTexture3d() returned;
 *   - the sequences of lookups/stores are equal across the backends;
 *   - the result flag is 1 (found / written).
 *
 * Mode "noservices": libshader::texture3dAvailable() with no services (a context that never received
 * a state, as test_context_no_services.cpp builds): false each time, and one warning for each kind
 * (texture3d, bake3d), however often it is asked.
 *
 * It is a separate executable from test_texture_substitute.cpp because the 3D forwarders appear in the
 * services interface only in Phase 7 (T085), so one executable could not compile in Phase 6. The
 * .rslo half runs with OPENRENDER_ENABLE_JIT=OFF; the .slo half and the cross-backend check are
 * compiled in only when the JIT is on. Red: does not compile until Phase 7; enabled by T089.
 */

#include "recording_texture_system.h"
#include "texture_substitute_harness.h"

#include "common/diagnostics.h"
#include "libshader/shading/textureCalls.h"

#include <atomic>

class CCountingSink : public CDiagnosticSink {
    public:
        std::atomic<int> notStarted{0};
        std::atomic<int> other{0};
        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            if (code == CODE_NOTSTARTED && severity == DIAGNOSTIC_WARNING)
                ++notStarted;
            else
                ++other;
            fprintf(stderr, "  diagnostic (code %d, severity %d): %s", (int)code, (int)severity, message);
        }
};

struct SRun3d {
        SRunResult out;
        std::vector<CTextureServices::S3DCall> calls;
};

static SRun3d run3d(CProbeContext *probe, CTextureServices *services, CProgrammableShaderInstance *shader, CSurface *object) {
    services->calls3d.clear();
    SRun3d r;
    r.out = runSurface(probe, shader, object);
    r.calls = services->calls3d;
    return r;
}

static std::vector<CTextureServices::S3DCall> only(const SRun3d &r, const char *what) {
    std::vector<CTextureServices::S3DCall> v;
    for (const auto &c : r.calls)
        if (c.what == what)
            v.push_back(c);
    return v;
}

// One backend's run of one probe: the checks that do not depend on the other backend
static void checkRun(const SRun3d &r, const char *what, const char *op, CTexture3d *token) {
    EXPECT_TRUE(!only(r, "resolve").empty());
    for (const auto &c : only(r, "resolve")) {
        EXPECT_TRUE(c.texture == token);
        EXPECT_TRUE(c.n == 0); // no user channels
    }
    const auto ops = only(r, op);
    if (ops.size() != (size_t)kNumPoints)
        fprintf(stderr, "    %s: %zu %s call(s), expected %d\n", what, ops.size(), op, kNumPoints);
    EXPECT_TRUE(ops.size() == (size_t)kNumPoints);
    for (size_t i = 0; i < ops.size() && i < (size_t)kNumPoints; i++) {
        EXPECT_TRUE(ops[i].texture == token);
        for (int a = 0; a < 3; a++) {
            EXPECT_TRUE(nearlyEqual(ops[i].P[a], kP[i][a]));
            EXPECT_TRUE(nearlyEqual(ops[i].N[a], kN[i][a]));
        }
        EXPECT_TRUE(nearlyEqual(ops[i].radius, 0.f)); // du = dv = 0
        EXPECT_TRUE(nearlyEqual(r.out.ci[i][0], 1.f)); // found / written
    }
    // texture3d() never stores and bake3d() never looks up
    EXPECT_TRUE(only(r, strcmp(op, "lookup") == 0 ? "store" : "lookup").empty());
}

static bool sameSequence(const SRun3d &a, const SRun3d &b, const char *op) {
    const auto x = only(a, op), y = only(b, op);
    if (x.size() != y.size())
        return false;
    for (size_t i = 0; i < x.size(); i++) {
        for (int k = 0; k < 3; k++)
            if (!nearlyEqual(x[i].P[k], y[i].P[k]) || !nearlyEqual(x[i].N[k], y[i].N[k]))
                return false;
        if (!nearlyEqual(x[i].radius, y[i].radius) || x[i].texture != y[i].texture)
            return false;
    }
    return true;
}

int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "builtins";
    if (mode != "builtins" && mode != "noservices") {
        fprintf(stderr, "usage: %s [builtins|noservices]\n", argv[0]);
        return 2;
    }

    // ------------------------------------------------------------------------------------------
    // No services: the availability helper, with no RiBegin and a context that never took a state
    // ------------------------------------------------------------------------------------------
    if (mode == "noservices") {
        EXPECT_TRUE(CShadingContext::defaultServices() == nullptr);
        CCountingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);

        CProbeContext *context = new CProbeContext(0); // no updateState(): no state, no services needed
        for (int pass = 0; pass < 3; pass++) {
            EXPECT_TRUE(!libshader::texture3dAvailable(context, "texture3d"));
            EXPECT_TRUE(!libshader::texture3dAvailable(context, "bake3d"));
        }
        EXPECT_TRUE(sink.notStarted == 2); // one warning per kind, however often it is asked
        EXPECT_TRUE(sink.other == 0);
        delete context;

        installDiagnosticSink(previous);
        printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
        return g_failed > 0 ? 1 : 0;
    }

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *shaderDir = getenv("SUBSTITUTE_SHADER_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(shaderDir != nullptr);
    if (!oshaderBin || !shaderDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_texture_substitute_3d_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

#ifdef TEXTURE_SUBSTITUTE_HAVE_JIT
    const bool haveJit = true;
#else
    const bool haveJit = false;
    printf("JIT not built: the .slo half and the cross-backend check are skipped\n");
#endif

    RiBegin(RI_NULL);
    RiWorldBegin();

    CRendererServices *original = CShadingContext::defaultServices();
    EXPECT_TRUE(original != nullptr);
    CRecordingTextureSystem unused; // texture3d() does not touch the texture system
    CTextureServices services(original, &unused);
    CShadingContext::setDefaultServices(&services);

    CAttributes *attrs = new CAttributes();
    CXform *xform = new CXform();
    CProbeSurface *object = new CProbeSurface(attrs, xform);
    CProbeContext *probe = new CProbeContext(0);
    probe->updateState();

    // The token getTexture3d() returns
    CTexture3d *expected = services.getTexture3d("anything", FALSE, nullptr, nullptr, nullptr);
    EXPECT_TRUE(expected != nullptr);

    struct SProbe {
            const char *name, *op;
    };
    for (const SProbe &p : {SProbe{"texture3d", "lookup"}, SProbe{"bake3d", "store"}}) {
        CProgrammableShaderInstance *rslo = compileAndLoad(oshaderBin, shaderDir, dir, p.name, "rslo");
        CProgrammableShaderInstance *slo = haveJit ? compileAndLoad(oshaderBin, shaderDir, dir, p.name, "slo") : nullptr;
        if (!rslo)
            continue;

        printf("%s: the interpreter's calls\n", p.name);
        const SRun3d ir = run3d(probe, &services, rslo, object);
        checkRun(ir, "interpreter", p.op, expected);

        if (slo) {
            printf("%s: the JIT's calls, and the sequences equal across backends\n", p.name);
            const SRun3d jr = run3d(probe, &services, slo, object);
            checkRun(jr, "JIT", p.op, expected);
            EXPECT_TRUE(sameSequence(ir, jr, p.op));
        }
    }

    delete probe;
    CShadingContext::setDefaultServices(original);
    RiWorldEnd();
    RiEnd();

    if (chdir(savedCwd) != 0)
        perror("chdir (restoring working directory)");
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
