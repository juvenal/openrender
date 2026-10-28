/**
 * tests/shading_parity/test_texture_substitute.cpp
 *
 * ShadingParity_TextureSubstitute (spec 027, T-S1 part one, T026, FR-012, FR-018(b),
 * User Story 3 AS1-AS4, SC-008).
 *
 * Shading reaches textures only through the renderer services' texture system. This test installs
 * a recording substitute (recording_texture_system.h) after RiBegin(), runs one probe shader per
 * builtin as .rslo (the interpreter) and as .slo (the JIT) on a four-point state, and checks:
 *
 *   A. each backend's recorded request equals the interpreter's expected request (from Phase 9, FR-010,
 *      FR-022: the JIT binds the parameter list and builds the footprint as the interpreter does): the
 *      handle request, the four corners (the collapsed, du = dv = 0 derivatives, or the eight given
 *      coordinates), and the option block of the shader's parameter list (init() defaults for what the
 *      list leaves alone). Before Phase 9 the JIT's request differed (a point lookup, the scratch's
 *      options, a color form that wrote one channel); T104 switched its expectations to "equal to the
 *      interpreter's", the one sanctioned assertion change, and enabled the parameter-list probes
 *      under the JIT.
 *   B. the handle requests (name, kind) and the textureinfo() request are equal across backends;
 *   C. every builtin returns the canned value;
 *   D. with the texture system removed (textureSystem() returns nullptr): each builtin returns its
 *      fill value, textureinfo() returns 0 and writes nothing, no call crashes, and each kind of
 *      lookup warns exactly once (texture, environment, shadow, textureinfo: four warnings in all,
 *      all different).
 *
 * Mode "hider" (ShadingParity_TextureSubstituteHiderNoSystem): the hiders' environment lookup. A
 * real render of occlusion() with "environmentmap" under services with no texture system returns
 * the fill-based result, makes no lookup, and warns "environment" exactly once.
 *
 * The .rslo half runs, and must pass, with OPENRENDER_ENABLE_JIT=OFF; the .slo half and the
 * cross-backend equality checks are compiled in only when the JIT is on and reported as skipped
 * otherwise. Red: the executable cannot be built until CRendererServices has textureSystem() and
 * the 3D forwarders (Phase 6, T072); enabled by T082.
 *
 * Inputs (environment): OSHADER_BIN, SUBSTITUTE_SHADER_DIR (this directory, with the probes), and
 * for the hider mode DISPLAYS.
 */

#include "recording_texture_system.h"
#include "texture_substitute_harness.h"

#include "common/diagnostics.h"

#include <atomic>
#include <set>

// Counts NOTSTARTED warnings and keeps their text
class CWarningSink : public CDiagnosticSink {
    public:
        std::atomic<int> notStarted{0};
        std::atomic<int> other{0};
        std::vector<std::string> notStartedMessages;

        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            if (code == CODE_NOTSTARTED && severity == DIAGNOSTIC_WARNING) {
                ++notStarted;
                notStartedMessages.push_back(message);
            }
            else {
                ++other;
                fprintf(stderr, "  diagnostic (code %d, severity %d): %s", (int)code, (int)severity, message);
            }
        }
};

// What a run recorded
struct SRun {
        SRunResult out;
        std::vector<SRecordedTextureCall> calls;
};

static SRun run(CProbeContext *probe, CRecordingTextureSystem *recording, CProgrammableShaderInstance *shader, CSurface *object) {
    if (recording)
        recording->calls.clear();
    setupState(probe, object);
    SRun r;
    r.out = runSurface(probe, shader, object);
    if (recording)
        r.calls = recording->calls;
    return r;
}

static std::vector<SRecordedTextureCall> ofKind(const SRun &r, SRecordedTextureCall::EKind kind) {
    std::vector<SRecordedTextureCall> v;
    for (const SRecordedTextureCall &c : r.calls)
        if (c.kind == kind)
            v.push_back(c);
    return v;
}

static bool sameHandleRequests(const SRun &a, const SRun &b) {
    const auto x = ofKind(a, SRecordedTextureCall::Handle), y = ofKind(b, SRecordedTextureCall::Handle);
    if (x.size() != y.size())
        return false;
    for (size_t i = 0; i < x.size(); i++)
        if (x[i].name != y[i].name || x[i].handleKind != y[i].handleKind)
            return false;
    return true;
}

static bool isColor(const float *c, float r, float g, float b) { return nearlyEqual(c[0], r) && nearlyEqual(c[1], g) && nearlyEqual(c[2], b); }

// ---- expectations on one recorded call -------------------------------------------------------

static void expectOptions(const TextureOpt &o, float blur, float width, float swidth, float twidth, float fill, float samples, RtFilterFunc filter,
                          const char *what) {
    const bool ok = nearlyEqual(o.blur, blur) && nearlyEqual(o.width, width) && nearlyEqual(o.swidth, swidth) && nearlyEqual(o.twidth, twidth) &&
                    nearlyEqual(o.fill, fill) && nearlyEqual(o.samples, samples) && (filter == nullptr || o.filter == filter);
    if (!ok)
        fprintf(stderr, "    %s: options blur %g width %g swidth %g twidth %g fill %g samples %g (expected %g %g %g %g %g %g)\n", what, o.blur,
                o.width, o.swidth, o.twidth, o.fill, o.samples, blur, width, swidth, twidth, fill, samples);
    EXPECT_TRUE(ok);
}

static void expectTrace(const TextureOpt &o, float traceSamples, const float *bias, const char *what) {
    const bool ok = nearlyEqual(o.traceSamples, traceSamples) && (bias == nullptr || nearlyEqual(o.bias, *bias));
    if (!ok)
        fprintf(stderr, "    %s: traceSamples %g bias %g (expected %g%s)\n", what, o.traceSamples, o.bias, traceSamples, bias ? "" : ", bias not checked");
    EXPECT_TRUE(ok);
}

static void expectHandleRequest(const SRun &r, const char *name, ETextureKind kind, const char *what) {
    const auto handles = ofKind(r, SRecordedTextureCall::Handle);
    const bool ok = handles.size() == 1 && handles[0].name == name && handles[0].handleKind == kind;
    if (!ok)
        fprintf(stderr, "    %s: %zu handle request(s), expected one for \"%s\"\n", what, handles.size(), name);
    EXPECT_TRUE(ok);
}

// ---- the interpreter's expected requests -----------------------------------------------------

static void checkInterpreterTexture(const SRun &r, const char *what, bool eight, float blur, float width, float fill, float samples, RtFilterFunc filter,
                                    bool floatChannel1) {
    expectHandleRequest(r, "tex_a.tex", ETextureKind::Texture, what);
    const auto calls = ofKind(r, SRecordedTextureCall::Texture);
    EXPECT_TRUE(calls.size() == (size_t)kNumPoints);
    EXPECT_TRUE(ofKind(r, SRecordedTextureCall::Point).empty());
    for (size_t i = 0; i < calls.size() && i < (size_t)kNumPoints; i++) {
        const SRecordedTextureCall &c = calls[i];
        EXPECT_TRUE(c.name == "tex_a.tex");
        EXPECT_TRUE(c.perthread != nullptr);
        for (int k = 0; k < 4; k++) {
            const float ds = eight ? ((k & 1) ? 0.1f : 0.f) : 0.f;
            const float dt = eight ? ((k & 2) ? 0.2f : 0.f) : 0.f;
            EXPECT_TRUE(nearlyEqual(c.s[k], kS[i] + ds));
            EXPECT_TRUE(nearlyEqual(c.t[k], kT[i] + dt));
        }
        expectOptions(c.opt, blur, width, 1, 1, fill, samples, filter, what);
        const float *ci = r.out.ci[i];
        if (floatChannel1)
            EXPECT_TRUE(isColor(ci, CRecordingTextureSystem::kTexture[1], CRecordingTextureSystem::kTexture[1], CRecordingTextureSystem::kTexture[1]));
        else
            EXPECT_TRUE(isColor(ci, CRecordingTextureSystem::kTexture[0], CRecordingTextureSystem::kTexture[1], CRecordingTextureSystem::kTexture[2]));
    }
}

static void checkInterpreterEnvironment(const SRun &r, const char *what, const char *name, const float (*dirs)[3], float blur, float fill,
                                        float traceSamples, const float *bias, RtFilterFunc filter) {
    expectHandleRequest(r, name, ETextureKind::Environment, what);
    const auto calls = ofKind(r, SRecordedTextureCall::Environment);
    EXPECT_TRUE(calls.size() == (size_t)kNumPoints);
    for (size_t i = 0; i < calls.size() && i < (size_t)kNumPoints; i++) {
        const SRecordedTextureCall &c = calls[i];
        EXPECT_TRUE(c.name == name);
        EXPECT_TRUE(c.perthread != nullptr);
        for (int k = 0; k < 4; k++)
            for (int a = 0; a < 3; a++)
                EXPECT_TRUE(nearlyEqual(c.d[k][a], dirs[i][a]));
        expectOptions(c.opt, blur, 1, 1, 1, fill, 1, filter, what);
        expectTrace(c.opt, traceSamples, bias, what);
    }
}

int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "builtins";
    if (mode != "builtins" && mode != "hider") {
        fprintf(stderr, "usage: %s [builtins|hider]\n", argv[0]);
        return 2;
    }

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *shaderDir = getenv("SUBSTITUTE_SHADER_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(shaderDir != nullptr);
    if (!oshaderBin || !shaderDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_texture_substitute_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    // cwd is searched first for shaders, so the freshly compiled probes load
    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    CWarningSink sink;

    // =========================================================================================
    // Mode "hider": a real render, no texture system
    // =========================================================================================
    if (mode == "hider") {
        EXPECT_TRUE(writeTextFile(dir + "/tsub_hider.sl",
                                  "surface tsub_hider()\n{\n"
                                  "    float o = occlusion(P, N, 4, \"environmentmap\", \"env_a.env\");\n"
                                  "    Ci = color(o, o, o);\n    Oi = 1;\n}\n"));
        EXPECT_TRUE(runOshader(oshaderBin, dir + "/tsub_hider.sl", dir + "/tsub_hider.rslo", false));

        RiBegin(RI_NULL);
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        CRendererServices *original = CShadingContext::defaultServices();
        EXPECT_TRUE(original != nullptr);
        CTextureServices noSystem(original, nullptr);
        CShadingContext::setDefaultServices(&noSystem);

        int one = 1;
        RiFormat(16, 16, 1);
        RiDisplay((RtToken) "tsub_hider.tif", (RtToken) "file", (RtToken) "rgba", RI_NULL);
        RiProjection((RtToken) "perspective", RI_NULL);
        RiOption((RtToken) "limits", (RtToken) "numthreads", &one, RI_NULL);
        RiWorldBegin();
        RiTranslate(0, 0, 3);
        RiSurface((RtToken) "tsub_hider", RI_NULL);
        RiSphere(1, -1, 1, 360, RI_NULL);
        RiWorldEnd();

        // The services outlive the frame; put the original back before RiEnd()
        if (CShadingContext::defaultServices() == &noSystem)
            CShadingContext::setDefaultServices(original);
        installDiagnosticSink(previous);
        RiEnd();

        printf("hider path, no texture system: %d NOTSTARTED warning(s)\n", sink.notStarted.load());
        EXPECT_TRUE(sink.notStarted == 1);
        EXPECT_TRUE(sink.notStartedMessages.size() == 1 && sink.notStartedMessages[0].find("environment") != std::string::npos);
        EXPECT_TRUE(noSystem.calls3d.empty());

        if (chdir(savedCwd) != 0)
            perror("chdir (restoring working directory)");
        printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
        return g_failed > 0 ? 1 : 0;
    }

    // =========================================================================================
    // Mode "builtins"
    // =========================================================================================
    struct SProbe {
            const char *name;
            bool jit; // also run as .slo
    };
    static const SProbe probes[] = {
        {"tex_c", true},     {"tex_f", true},      {"env_c", true},        {"env_f", true},      {"shadow_f", true},
        {"textureinfo", true}, {"tex_c_pl", true},   {"tex_c8_pl", true}, {"env_c_pl", true}, {"shadow_f_pl", true},
    };

#ifdef TEXTURE_SUBSTITUTE_HAVE_JIT
    const bool haveJit = true;
#else
    const bool haveJit = false;
    printf("JIT not built: the .slo half and the cross-backend checks are skipped\n");
#endif

    RiBegin(RI_NULL);
    RiWorldBegin();

    CDiagnosticSink *previousSink = installDiagnosticSink(&sink);
    CRendererServices *original = CShadingContext::defaultServices();
    EXPECT_TRUE(original != nullptr);

    CAttributes *attrs = new CAttributes();
    CXform *xform = new CXform();
    CProbeSurface *object = new CProbeSurface(attrs, xform);

    // Compile and load every probe once
    std::vector<CProgrammableShaderInstance *> rslo, slo;
    for (const SProbe &p : probes) {
        rslo.push_back(compileAndLoad(oshaderBin, shaderDir, dir, p.name, "rslo"));
        slo.push_back((p.jit && haveJit) ? compileAndLoad(oshaderBin, shaderDir, dir, p.name, "slo") : nullptr);
    }

    // ---- A-C: the substitute installed ------------------------------------------------------
    {
        CRecordingTextureSystem recording;
        CTextureServices services(original, &recording);
        CShadingContext::setDefaultServices(&services);

        CProbeContext *probe = new CProbeContext(0);
        probe->updateState();

        std::vector<SRun> ir(sizeof(probes) / sizeof(probes[0])), jr(ir.size());
        for (size_t k = 0; k < ir.size(); k++) {
            if (rslo[k])
                ir[k] = run(probe, &recording, rslo[k], object);
            if (slo[k])
                jr[k] = run(probe, &recording, slo[k], object);
        }
        auto idx = [&](const char *n) {
            for (size_t k = 0; k < ir.size(); k++)
                if (strcmp(probes[k].name, n) == 0)
                    return k;
            return (size_t)0;
        };
        const RtFilterFunc defaultFilter = nullptr; // the lookups' own default filter: not pinned here

        printf("A. the interpreter's requests (.rslo)\n");
        checkInterpreterTexture(ir[idx("tex_c")], "tex_c .rslo", false, 0, 1, 0, 1, defaultFilter, false);
        checkInterpreterTexture(ir[idx("tex_f")], "tex_f .rslo", false, 0, 1, 0, 1, defaultFilter, true);
        checkInterpreterTexture(ir[idx("tex_c_pl")], "tex_c_pl .rslo", false, 0.02f, 2, 0.5f, 4, RiBoxFilter, false);
        checkInterpreterTexture(ir[idx("tex_c8_pl")], "tex_c8_pl .rslo", true, 0, 1, 0, 3, RiBoxFilter, false); // "samples" 3
        checkInterpreterEnvironment(ir[idx("env_c")], "env_c .rslo", "env_a.env", kN, 0, 0, 1, nullptr, defaultFilter);
        checkInterpreterEnvironment(ir[idx("env_f")], "env_f .rslo", "env_a.env", kN, 0, 0, 1, nullptr, defaultFilter);
        checkInterpreterEnvironment(ir[idx("env_c_pl")], "env_c_pl .rslo", "env_a.env", kN, 0.03f, 0.6f, 5, nullptr, RiBoxFilter);
        {
            const float bias = 0.25f;
            checkInterpreterEnvironment(ir[idx("shadow_f")], "shadow_f .rslo", "shadow_a.sm", kP, 0, 0, 1, nullptr, defaultFilter);
            checkInterpreterEnvironment(ir[idx("shadow_f_pl")], "shadow_f_pl .rslo", "shadow_a.sm", kP, 0.04f, 0, 3, &bias, RiBoxFilter);
        }
        for (const char *n : {"env_c", "env_c_pl"})
            for (int i = 0; i < kNumPoints; i++)
                EXPECT_TRUE(isColor(ir[idx(n)].out.ci[i], CRecordingTextureSystem::kEnvironment[0], CRecordingTextureSystem::kEnvironment[1],
                                    CRecordingTextureSystem::kEnvironment[2]));
        for (const char *n : {"env_f", "shadow_f", "shadow_f_pl"})
            for (int i = 0; i < kNumPoints; i++)
                EXPECT_TRUE(isColor(ir[idx(n)].out.ci[i], 0.375f, 0.375f, 0.375f)); // canned channels are equal: any reduction gives 0.375

        // textureinfo(): one describe, the canned resolution
        {
            const SRun &r = ir[idx("textureinfo")];
            expectHandleRequest(r, "tex_a.tex", ETextureKind::Info, "textureinfo .rslo");
            EXPECT_TRUE(!ofKind(r, SRecordedTextureCall::Describe).empty());
            for (int i = 0; i < kNumPoints; i++)
                EXPECT_TRUE(isColor(r.out.ci[i], 1, CRecordingTextureSystem::kResolution[0], CRecordingTextureSystem::kResolution[1]));
        }

        if (haveJit) {
            // The JIT's request is the interpreter's (Phase 9, T104): the same handle request, corners and options
            printf("A. the JIT's requests (.slo), equal to the interpreter's\n");
            checkInterpreterTexture(jr[idx("tex_c")], "tex_c .slo", false, 0, 1, 0, 1, defaultFilter, false);
            checkInterpreterTexture(jr[idx("tex_f")], "tex_f .slo", false, 0, 1, 0, 1, defaultFilter, true);
            checkInterpreterTexture(jr[idx("tex_c_pl")], "tex_c_pl .slo", false, 0.02f, 2, 0.5f, 4, RiBoxFilter, false);
            checkInterpreterTexture(jr[idx("tex_c8_pl")], "tex_c8_pl .slo", true, 0, 1, 0, 3, RiBoxFilter, false);
            checkInterpreterEnvironment(jr[idx("env_c")], "env_c .slo", "env_a.env", kN, 0, 0, 1, nullptr, defaultFilter);
            checkInterpreterEnvironment(jr[idx("env_f")], "env_f .slo", "env_a.env", kN, 0, 0, 1, nullptr, defaultFilter);
            checkInterpreterEnvironment(jr[idx("env_c_pl")], "env_c_pl .slo", "env_a.env", kN, 0.03f, 0.6f, 5, nullptr, RiBoxFilter);
            {
                const float bias = 0.25f;
                checkInterpreterEnvironment(jr[idx("shadow_f")], "shadow_f .slo", "shadow_a.sm", kP, 0, 0, 1, nullptr, defaultFilter);
                checkInterpreterEnvironment(jr[idx("shadow_f_pl")], "shadow_f_pl .slo", "shadow_a.sm", kP, 0.04f, 0, 3, &bias, RiBoxFilter);
            }
            for (const char *n : {"env_c", "env_c_pl"})
                for (int i = 0; i < kNumPoints; i++)
                    EXPECT_TRUE(isColor(jr[idx(n)].out.ci[i], CRecordingTextureSystem::kEnvironment[0], CRecordingTextureSystem::kEnvironment[1],
                                        CRecordingTextureSystem::kEnvironment[2]));
            for (const char *n : {"env_f", "shadow_f", "shadow_f_pl"})
                for (int i = 0; i < kNumPoints; i++)
                    EXPECT_TRUE(isColor(jr[idx(n)].out.ci[i], 0.375f, 0.375f, 0.375f));
            {
                const SRun &r = jr[idx("textureinfo")];
                expectHandleRequest(r, "tex_a.tex", ETextureKind::Info, "textureinfo .slo");
                EXPECT_TRUE(!ofKind(r, SRecordedTextureCall::Describe).empty());
                for (int i = 0; i < kNumPoints; i++)
                    EXPECT_TRUE(isColor(r.out.ci[i], 1, CRecordingTextureSystem::kResolution[0], CRecordingTextureSystem::kResolution[1]));
            }

            printf("B. the handle requests and the results equal across backends\n");
            for (const char *n : {"tex_c", "tex_f", "env_c", "env_f", "shadow_f", "textureinfo", "tex_c_pl", "tex_c8_pl", "env_c_pl", "shadow_f_pl"}) {
                EXPECT_TRUE(sameHandleRequests(ir[idx(n)], jr[idx(n)]));
                for (int i = 0; i < kNumPoints; i++)
                    for (int c = 0; c < 3; c++)
                        EXPECT_TRUE(nearlyEqual(ir[idx(n)].out.ci[i][c], jr[idx(n)].out.ci[i][c])); // C: the same canned value
            }
        }

        delete probe;
        CShadingContext::setDefaultServices(original);
    }

    // ---- D: no texture system ----------------------------------------------------------------
    {
        CTextureServices services(original, nullptr);
        CShadingContext::setDefaultServices(&services);

        CProbeContext *probe = new CProbeContext(0);
        probe->updateState();

        printf("D. no texture system: fill values, one warning per kind\n");
        for (size_t k = 0; k < sizeof(probes) / sizeof(probes[0]); k++) {
            const std::string n = probes[k].name;
            for (int backend = 0; backend < 2; backend++) {
                CProgrammableShaderInstance *shader = backend == 0 ? rslo[k] : slo[k];
                if (!shader)
                    continue;
                const SRun r = run(probe, nullptr, shader, object);
                const float fill = (n == "tex_c_pl" ? 0.5f : n == "env_c_pl" ? 0.6f : 0.f);
                for (int i = 0; i < kNumPoints; i++) {
                    if (n == "textureinfo")
                        EXPECT_TRUE(isColor(r.out.ci[i], 0, -1, -1)); // found 0, the result array untouched
                    else
                        EXPECT_TRUE(isColor(r.out.ci[i], fill, fill, fill));
                }
            }
        }
        delete probe;
        CShadingContext::setDefaultServices(original);
    }
    printf("   %d NOTSTARTED warning(s) in all\n", sink.notStarted.load());
    EXPECT_TRUE(sink.notStarted == 4); // texture, environment, shadow, textureinfo: once each, per process
    EXPECT_TRUE(std::set<std::string>(sink.notStartedMessages.begin(), sink.notStartedMessages.end()).size() == 4);

    installDiagnosticSink(previousSink);
    RiWorldEnd();
    RiEnd();

    if (chdir(savedCwd) != 0)
        perror("chdir (restoring working directory)");
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
