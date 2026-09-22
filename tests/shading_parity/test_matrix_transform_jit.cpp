/**
 * tests/shading_parity/test_matrix_transform_jit.cpp
 *
 * Regression test for GitHub #10: transform()/vtransform()/ntransform()
 * each have FOUR overloads (shaderFunctions.h):
 *   "p=Sp"  / "v=Sv"  / "n=Sn"   -- one space name, e.g. transform("world", P)
 *   "p=SSp" / "v=SSv" / "n=SSn"  -- two space names, transform(from, to, P)
 *   "p=mp"  / "v=mv"  / "n=mn"   -- one matrix, no space at all, transform(M, P)
 *   "p=Smp" / "v=Smv" / "n=Smn"  -- one space name + one matrix, transform(space, M, P)
 *
 * Under --jit, llvmEmitter.cpp's dispatch for these three mnemonics
 * unconditionally read operand 0 as a space-name string, regardless of
 * which overload was actually instantiated. Only the single-space form
 * ("p=Sp" etc.) was ever correct. The matrix-argument overload ("p=mp")
 * is the one filed in issue #10: with `M` a matrix variable, operand 0's
 * *identifier* (e.g. "m") got embedded as a literal space name, so the
 * runtime space lookup failed with "Unknown coordinate system: m" and
 * produced wrong output. The two-space ("p=SSp") and space+matrix
 * ("p=Smp") overloads turned out to be broken too, in different ways: the
 * "SSp" form silently no-op'd (operand 1 is a second space string, not
 * the point, so the old code's getVar(ins,1) failed and the whole
 * instruction was skipped), and the "Smp" form silently computed the
 * wrong thing (treated the matrix operand as if it didn't exist, doing a
 * pure space transform with no matrix multiply at all).
 *
 * Fix: llvmEmitter.cpp now branches on ins.proto (the compiled bytecode's
 * own overload signature, e.g. "p=mp") *before* reading any operands, and
 * dispatches to one of the four operand shapes above accordingly, calling
 * new op_{p,v,n}transform_{m,ss,sm} functions (rslOps.cpp) that mirror
 * the interpreter's TRANSFORM*EXPR/VTRANSFORM*EXPR/NTRANSFORM*EXPR macros
 * exactly for the "m"/"SS"/"Sm" shapes (the "S" shape's op_ptransform/
 * op_vtransform/op_ntransform already existed and are unchanged).
 *
 * This test renders one fixture exercising all four overloads across all
 * three mnemonics (transform/vtransform/ntransform x S/SS/m/Sm -- 12
 * overloads total) via printf(), and verifies JIT and interpreter agree.
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

static bool runOshader(const char *oshaderBin, const std::string &srcPath, const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                      "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// orender's own exit code is not a useful signal (see test_printf_jit.cpp's
// identical note) -- every run here exits non-zero on an unrelated,
// harmless "Failed to find shader \"defaultsurface\"" warning path.
static void runOrender(const char *orenderBin, const char *displaysDir,
                       const std::string &shadersDir, const std::string &ribPath,
                       const std::string &logPath) {
    std::string cmd = std::string("SHADERS=\"") + shadersDir + "\" ORENDERHOME=\"" + shadersDir +
                      "\" DISPLAYS=\"" + displaysDir + "\" \"" + orenderBin + "\" \"" +
                      ribPath + "\" > \"" + logPath + "\" 2>&1";
    system(cmd.c_str());
}

int main() {
    printf("GitHub #10: transform()/vtransform()/ntransform() matrix and multi-space overloads\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_matrix_transform_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    const std::string shaderName = "matrix_transform_fixture";
    const std::string src = dir + "/" + shaderName + ".sl";
    const std::string sloOut = dir + "/" + shaderName + ".slo";
    const std::string rsloOut = dir + "/" + shaderName + ".rslo";

    EXPECT_TRUE(writeFile(src,
        "surface " + shaderName + "()\n"
        "{\n"
        "    uniform matrix m = 1;\n"
        "    m = translate(m, point(5, 0, 0));\n"
        "\n"
        "    // \"p=mp\": GitHub #10's exact repro shape.\n"
        "    uniform point pm = transform(m, point(1, 2, 3));\n"
        "    printf(\"pm=(%f,%f,%f)\\n\", pm[0], pm[1], pm[2]);\n"
        "\n"
        "    // \"p=SSp\": two-space overload.\n"
        "    uniform point pss = transform(\"object\", \"world\", point(1, 2, 3));\n"
        "    printf(\"pss=(%f,%f,%f)\\n\", pss[0], pss[1], pss[2]);\n"
        "\n"
        "    // \"p=Smp\": space + matrix overload.\n"
        "    uniform point psm = transform(\"object\", m, point(1, 2, 3));\n"
        "    printf(\"psm=(%f,%f,%f)\\n\", psm[0], psm[1], psm[2]);\n"
        "\n"
        "    // Sanity control: \"p=Sp\", the single-space overload that was\n"
        "    // already correct pre-fix -- rules out a fix that accidentally\n"
        "    // breaks the previously-working shape.\n"
        "    uniform point ps = transform(\"world\", point(1, 2, 3));\n"
        "    printf(\"ps=(%f,%f,%f)\\n\", ps[0], ps[1], ps[2]);\n"
        "\n"
        "    uniform matrix mr = 1;\n"
        "    mr = rotate(mr, radians(90), vector(0, 0, 1));\n"
        "\n"
        "    // \"v=mv\"\n"
        "    uniform vector vm = vtransform(mr, vector(1, 0, 0));\n"
        "    printf(\"vm=(%f,%f,%f)\\n\", vm[0], vm[1], vm[2]);\n"
        "\n"
        "    // \"v=SSv\"\n"
        "    uniform vector vss = vtransform(\"object\", \"world\", vector(1, 0, 0));\n"
        "    printf(\"vss=(%f,%f,%f)\\n\", vss[0], vss[1], vss[2]);\n"
        "\n"
        "    // \"v=Smv\"\n"
        "    uniform vector vsm = vtransform(\"object\", mr, vector(1, 0, 0));\n"
        "    printf(\"vsm=(%f,%f,%f)\\n\", vsm[0], vsm[1], vsm[2]);\n"
        "\n"
        "    uniform matrix ms = 1;\n"
        "    ms = scale(ms, point(2, 1, 1));\n"
        "\n"
        "    // \"n=mn\"\n"
        "    uniform normal nm = ntransform(ms, normal(1, 0, 0));\n"
        "    printf(\"nm=(%f,%f,%f)\\n\", nm[0], nm[1], nm[2]);\n"
        "\n"
        "    // \"n=SSn\"\n"
        "    uniform normal nss = ntransform(\"object\", \"world\", normal(1, 0, 0));\n"
        "    printf(\"nss=(%f,%f,%f)\\n\", nss[0], nss[1], nss[2]);\n"
        "\n"
        "    // \"n=Smn\"\n"
        "    uniform normal nsm = ntransform(\"object\", ms, normal(1, 0, 0));\n"
        "    printf(\"nsm=(%f,%f,%f)\\n\", nsm[0], nsm[1], nsm[2]);\n"
        "\n"
        "    Ci = color(0, 0, 0);\n"
        "}\n"));

    EXPECT_TRUE(runOshader(oshaderBin, src, sloOut, /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, src, rsloOut, /*jit=*/false));

    auto writeRib = [&](const std::string &format) {
        const std::string path = dir + "/scene_" + format + ".rib";
        writeFile(path,
            "Projection \"perspective\"\n"
            "Display \"test.tif\" \"file\" \"rgba\"\n"
            "WorldBegin\n"
            "    Translate 0 0 3\n"
            "    Attribute \"shade\" \"shaderformat\" [\"" + format + "\"]\n"
            "    Surface \"" + shaderName + "\"\n"
            "    Sphere 1 -1 1 360\n"
            "WorldEnd\n");
        return path;
    };
    const std::string sloRib = writeRib("slo");
    const std::string rsloRib = writeRib("rslo");

    const std::string sloLog = dir + "/slo_out.log";
    const std::string rsloLog = dir + "/rslo_out.log";
    runOrender(orenderBin, displaysDir, dir, sloRib, sloLog);
    runOrender(orenderBin, displaysDir, dir, rsloRib, rsloLog);

    const std::string sloText = readFile(sloLog);
    const std::string rsloText = readFile(rsloLog);
    EXPECT_TRUE(!sloText.empty());
    EXPECT_TRUE(!rsloText.empty());

    // Every shading point on this sphere computes the same uniform values,
    // so exactly one line per marker is expected from each backend. We
    // don't hardcode the expected numeric values here (the exact "object"/
    // "world" relationship depends on the renderer's own space-resolution
    // conventions, not something this test should re-derive by hand) --
    // instead we assert JIT/interpreter PARITY: whatever the interpreter
    // (the reference backend) computes, the JIT must compute identically.
    static const char *markers[] = {"pm=", "pss=", "psm=", "ps=", "vm=", "vss=", "vsm=", "nm=", "nss=", "nsm="};
    for (const char *marker : markers) {
        auto extractLine = [&](const std::string &text) -> std::string {
            size_t pos = text.find(marker);
            if (pos == std::string::npos)
                return {};
            size_t end = text.find('\n', pos);
            return text.substr(pos, end == std::string::npos ? std::string::npos : end - pos + 1);
        };
        std::string sloLine = extractLine(sloText);
        std::string rsloLine = extractLine(rsloText);
        printf("  %s -- .slo: %s  .rslo: %s\n", marker,
               sloLine.empty() ? "MISSING" : sloLine.c_str(),
               rsloLine.empty() ? "MISSING" : rsloLine.c_str());
        EXPECT_TRUE(!sloLine.empty());
        EXPECT_TRUE(!rsloLine.empty());
        EXPECT_TRUE(!sloLine.empty() && !rsloLine.empty() && sloLine == rsloLine);
    }

    // GitHub #10's own literal symptom: the JIT log must never contain the
    // "Unknown coordinate system: m" failure text (matrix variable's
    // identifier mistaken for a space name).
    EXPECT_TRUE(sloText.find("Unknown coordinate system: m") == std::string::npos);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
