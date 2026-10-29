/**
 * tests/shading_parity/test_jit_paramlist_diagnostics_texture.cpp
 *
 * ShadingParity_JitPLDiagnosticsTexture (spec 027, T-J3, T032, FR-022): the diagnostics of an
 * optional parameter list of texture(), environment() and shadow() are the interpreter's own under
 * the LLVM JIT too. Modelled on test_jit_paramlist_diagnostics.cpp (GitHub #55), which covers
 * trace()/occlusion(); a NEW test, so that test stays green and unchanged until Phase 9.
 *
 * Until Phase 9 the JIT's texture(), environment() and shadow() drop their parameter lists without
 * looking at them, so a misspelled or unknown parameter goes unreported under .slo while the
 * interpreter (.rslo) reports it. Phase 9 binds a call's list through the same CPLLookup::bind()
 * overrides the interpreter's plBegin() uses, once per call site.
 *
 * The fixture makes three calls, each with its own unknown parameter:
 *   - texture(..., "bogus27t", 1)
 *   - environment(..., "bogus27e", 1)
 *   - shadow(..., "bogus27s", 1)
 * Rendered single-threaded (one shading context, so one lookup per call site), each interpreter
 * diagnostic line must appear exactly once in each backend's log. Red on the starting tree: the .slo
 * log has none of them.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

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

static bool writeFile(const std::string &path, const std::string &content) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

static std::string readFile(const std::string &path) {
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

static int countOf(const std::string &text, const std::string &needle) {
    int n = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size()))
        n++;
    return n;
}

static bool runOshader(const char *oshaderBin, const std::string &srcPath, const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                      "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// orender's exit code is not a useful signal here (see test_printf_jit.cpp).
static void runOrender(const char *orenderBin, const char *displaysDir, const std::string &shadersDir,
                       const std::string &ribPath, const std::string &logPath) {
    std::string cmd = std::string("SHADERS=\"") + shadersDir + "\" ORENDERHOME=\"" + shadersDir +
                      "\" DISPLAYS=\"" + displaysDir + "\" \"" + orenderBin + "\" \"" +
                      ribPath + "\" > \"" + logPath + "\" 2>&1";
    int rc = system(cmd.c_str());
    (void)rc;
}

int main() {
    printf("Spec 027: texture()/environment()/shadow() parameter-list diagnostics under the interpreter and the JIT\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_jit_pl_diag_texture_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    const std::string shaderName = "jit_pl_diagnostics_texture_fixture";
    const std::string src = dir + "/" + shaderName + ".sl";
    EXPECT_TRUE(writeFile(src,
        "surface " + shaderName + "()\n"
        "{\n"
        "    vector Nf = faceforward(normalize(N), I);\n"
        "    vector R = reflect(normalize(I), Nf);\n"
        "    color c = texture(\"tex_a.tex\", s, t, \"bogus27t\", 1);\n"
        "    color e = environment(\"env_a.env\", R, \"bogus27e\", 1);\n"
        "    float h = shadow(\"shadow_a.sm\", P, \"bogus27s\", 1);\n"
        "    Ci = c + e + color(h, h, h);\n"
        "    Oi = 1;\n"
        "}\n"));
    EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + shaderName + ".slo", /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + shaderName + ".rslo", /*jit=*/false));

    struct Check {
            const char *what;
            const char *needle;
    };
    static const Check checks[] = {
        {"unknown texture() parameter", "Unknown parameter: \"bogus27t\""},
        {"unknown environment() parameter", "Unknown parameter: \"bogus27e\""},
        {"unknown shadow() parameter", "Unknown parameter: \"bogus27s\""},
    };

    for (const char *format : {"rslo", "slo"}) {
        const std::string rib = dir + "/scene_" + format + ".rib";
        writeFile(rib,
            "Option \"limits\" \"numthreads\" [1]\n"
            "Projection \"perspective\"\n"
            "Format 16 16 1\n"
            "Display \"" + dir + "/diagtex_" + format + ".tif\" \"file\" \"rgba\"\n"
            "WorldBegin\n"
            "    Translate 0 0 3\n"
            "    Attribute \"shade\" \"shaderformat\" [\"" + format + "\"]\n"
            "    Surface \"" + shaderName + "\"\n"
            "    Sphere 1 -1 1 360\n"
            "WorldEnd\n");
        const std::string log = dir + "/" + format + "_out.log";
        runOrender(orenderBin, displaysDir, dir, rib, log);
        const std::string text = readFile(log);
        EXPECT_TRUE(!text.empty());
        for (const Check &c : checks) {
            const int n = countOf(text, c.needle);
            printf("  .%s  %-42s %d time(s)%s\n", format, c.what, n, n == 1 ? "" : "  -- WRONG, expected 1");
            EXPECT_TRUE(n == 1);
        }
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
