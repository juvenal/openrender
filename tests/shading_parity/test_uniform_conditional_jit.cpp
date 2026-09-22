/**
 * tests/shading_parity/test_uniform_conditional_jit.cpp
 *
 * Regression tests for GitHub #8 (root cause) and #6 (a symptom of the
 * same bug, not a separate one).
 *
 * GitHub #8: under --jit, an assignment to a UNIFORM variable inside an
 * `if` body executed unconditionally, regardless of whether the `if`
 * condition was true or false, as long as the condition was ALSO uniform.
 * Root cause: llvmEmitter.cpp's collapseArgs() takes a "fast path" whenever
 * an instruction's destination and every operand are uniform (stride 0):
 * it passes n=1 and tags=nullptr to the underlying op_* call. if/else
 * control flow is implemented entirely via runtime `tags` mutation
 * (op_if_update/op_else_update) in a "flat batch" model -- there is no
 * real LLVM branching, so nothing else gates whether a body's instructions
 * "should" run. A uniform-classified instruction lexically inside an
 * if/else body that hit collapseArgs's fast path discarded the real tags
 * array, so it ran unconditionally no matter what the enclosing if/else
 * actually decided.
 *
 * GitHub #6 ("ternary with a string-equality condition always evaluates
 * false") turned out to be the exact same bug wearing different clothes:
 * a ternary compiles to the identical if/else/endif opcode sequence as a
 * statement-level if (confirmed by diffing the compiled .rslo text of both
 * forms -- expression.cpp's CConditionalExpression::getCode() and
 * CIfThenElse::getCode() both emit opcodeIf/opcodeElse/opcodeEndif). An
 * if/else with BOTH branches assigning a uniform destination hits the bug
 * on BOTH branches: since neither respects the real tags array, both
 * assignments execute in program order regardless of the condition, and
 * the else-branch (always last) always wins -- which looks exactly like
 * "the condition always evaluates false", but has nothing to do with
 * string equality, seql, or ternary codegen specifically (confirmed: a
 * plain if/else with no ternary and no string comparison at all shows the
 * identical symptom).
 *
 * Fix: llvmEmitter.cpp tracks conditionalDepth, incremented/decremented at
 * every tags-masked scope (if/endif, illuminate/endilluminate,
 * solar/endsolar, the illuminance/gather loop body) -- NOT for/while loops,
 * which use real LLVM branching with no tags masking at all (confirmed
 * empirically: a uniform assignment inside a false-from-the-start `while`
 * already rendered correctly, pre-fix). collapseArgs's fast path is now
 * only taken at conditionalDepth == 0.
 *
 * This test compiles one fixture combining #8's exact shape (if, no else,
 * false condition) with #6's shapes (if/else with an explicit else, and a
 * ternary), all with uniform destinations and uniform conditions, and
 * verifies via printf() (fixed under JIT by #11/#13/#14, so safe to use
 * here as the observation mechanism) that JIT and interpreter agree and
 * both compute the RISpec-correct values.
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
    printf("GitHub #8/#6: uniform assignment inside a uniform-conditioned if/else or ternary\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_uniform_cond_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    // The shader's own name is the string-equality condition's operand
    // (matching GitHub #6's exact repro shape) -- the .sl filename stem
    // must equal this declared surface name for getShader()/the RIB's
    // Surface statement to find it.
    const std::string shaderName = "uniform_conditional_fixture";
    const std::string src = dir + "/" + shaderName + ".sl";
    const std::string sloOut = dir + "/" + shaderName + ".slo";
    const std::string rsloOut = dir + "/" + shaderName + ".rslo";

    EXPECT_TRUE(writeFile(src,
        "surface " + shaderName + "(uniform float trueCond = 1; uniform float falseCond = 0)\n"
        "{\n"
        "    // GitHub #8's exact shape: if with no else, false uniform\n"
        "    // condition -- the body must NOT execute.\n"
        "    uniform float a = 0;\n"
        "    if (falseCond > 0) {\n"
        "        a = 1;\n"
        "    }\n"
        "    printf(\"a=%f\\n\", a);\n"
        "\n"
        "    // GitHub #6: if/else (explicit else) with a uniform\n"
        "    // string-equality condition.\n"
        "    uniform float b;\n"
        "    if (shadername() == \"" + shaderName + "\") {\n"
        "        b = 1;\n"
        "    } else {\n"
        "        b = 0;\n"
        "    }\n"
        "    printf(\"b=%f\\n\", b);\n"
        "\n"
        "    // GitHub #6: ternary form of the same string-equality condition.\n"
        "    uniform float c = (shadername() == \"" + shaderName + "\") ? 1 : 0;\n"
        "    printf(\"c=%f\\n\", c);\n"
        "\n"
        "    // GitHub #6: ternary with a plain literal-vs-literal string\n"
        "    // condition, no function call at all.\n"
        "    uniform string lit = \"hello\";\n"
        "    uniform float d = (lit == \"hello\") ? 1 : 0;\n"
        "    printf(\"d=%f\\n\", d);\n"
        "\n"
        "    // Sanity control: the same if-with-no-else shape as `a`, but\n"
        "    // with a TRUE condition -- must still execute (rules out a\n"
        "    // fix that accidentally suppresses the if body entirely).\n"
        "    uniform float e = 0;\n"
        "    if (trueCond > 0) {\n"
        "        e = 1;\n"
        "    }\n"
        "    printf(\"e=%f\\n\", e);\n"
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
    // so exactly one line per marker is expected from each backend.
    struct Check {
            const char *marker;
            const char *expectedLine;
    };
    static const Check checks[] = {
        {"a=", "a=0.000000\n"}, // GitHub #8: must NOT have executed
        {"b=", "b=1.000000\n"}, // GitHub #6: if/else, condition true
        {"c=", "c=1.000000\n"}, // GitHub #6: ternary, condition true
        {"d=", "d=1.000000\n"}, // GitHub #6: literal-vs-literal ternary
        {"e=", "e=1.000000\n"}, // sanity control: if-no-else, condition true
    };
    for (const Check &c : checks) {
        bool sloOk = sloText.find(c.expectedLine) != std::string::npos;
        bool rsloOk = rsloText.find(c.expectedLine) != std::string::npos;
        printf("  %s -- .slo %s  .rslo %s\n", c.expectedLine,
               sloOk ? "OK" : "WRONG", rsloOk ? "OK" : "WRONG");
        EXPECT_TRUE(sloOk);
        EXPECT_TRUE(rsloOk);
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
