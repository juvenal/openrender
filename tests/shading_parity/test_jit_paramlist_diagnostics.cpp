/**
 * tests/shading_parity/test_jit_paramlist_diagnostics.cpp
 *
 * GitHub #55: the diagnostics of an optional parameter list are the
 * interpreter's own under the LLVM JIT too.
 *
 * Under --jit, trace()/transmission()/occlusion()/indirectdiffuse() used to
 * drop their parameter lists without looking at them, so a misspelled or
 * unknown parameter went unreported under .slo while the interpreter
 * (.rslo) reported it. The JIT now binds a call's list through the same
 * CPLLookup::bind() overrides the interpreter's plBegin() uses, once per
 * call site, so both backends print the same lines the same number of times.
 *
 * The fixture makes three calls:
 *   - trace(..., "bogus55", 1): CPLLookup::bind's "Unknown parameter" error.
 *   - trace(..., "samplebase", 2): also unknown, because CTraceLookup::bind
 *     spells it "sampleBase" (occlusion() and gather() spell it "samplebase").
 *     The JIT must reproduce the interpreter's spelling, not correct it.
 *   - occlusion(..., "bogus56", 1): COcclusionLookup::bind's warning for a
 *     uniform value it cannot bind as a channel.
 * Rendered single-threaded (one shading context, so one lookup per call
 * site), each line must appear exactly once in each backend's log.
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
    printf("GitHub #55: parameter-list diagnostics under the interpreter and the JIT\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_jit_pl_diag_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    const std::string shaderName = "jit_pl_diagnostics_fixture";
    const std::string src = dir + "/" + shaderName + ".sl";
    EXPECT_TRUE(writeFile(src,
        "surface " + shaderName + "()\n"
        "{\n"
        "    vector Nf = faceforward(normalize(N), I);\n"
        "    vector R = reflect(normalize(I), Nf);\n"
        "    color c = trace(P, R, \"bogus55\", 1, \"samplebase\", 2);\n"
        "    float o = occlusion(P, Nf, 4, \"bogus56\", 1);\n"
        "    Ci = c + color(o, o, o);\n"
        "    Oi = 1;\n"
        "}\n"));
    EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + shaderName + ".slo", /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + shaderName + ".rslo", /*jit=*/false));

    struct Check {
            const char *what;
            const char *needle;
    };
    static const Check checks[] = {
        {"unknown trace() parameter", "Unknown parameter: \"bogus55\""},
        {"trace() rejects the occlusion() spelling", "Unknown parameter: \"samplebase\""},
        {"uniform occlusion() parameter ignored", "parameter \"bogus56\" ignored"},
    };

    for (const char *format : {"rslo", "slo"}) {
        const std::string rib = dir + "/scene_" + format + ".rib";
        writeFile(rib,
            "Option \"limits\" \"numthreads\" [1]\n"
            "Projection \"perspective\"\n"
            "Format 16 16 1\n"
            "Display \"" + dir + "/diag_" + format + ".tif\" \"file\" \"rgba\"\n"
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
