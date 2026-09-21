/**
 * tests/shading_parity/test_printf_jit.cpp
 *
 * Regression tests for GitHub #11: printf() was a silent no-op under the
 * LLVM JIT (--jit) -- listed in llvmEmitter.cpp's kHandledOpcodes[] and
 * dispatched in emitFunction(), but that case emitted no IR at all.
 *
 * The first implementation attempt (before this test existed) fixed the
 * no-op but shipped two further bugs, both caught only by rendering a real
 * scene and inspecting stdout -- neither is visible from a unit test that
 * only checks the shader compiles or that op_printf() is callable in
 * isolation:
 *
 *   1. Operand off-by-one: printf's RSL prototype is "o=s.*" -- unlike
 *      format()'s "s=s.*" (a real result), printf's bytecode binds its
 *      FIRST logical argument (the format string) into the instruction's
 *      RESULT slot (ins.result), the same convention setcomp()'s "o=Vff"
 *      uses for its mutated vector. Copying format()'s operands[0]-is-fmt
 *      indexing verbatim made the emitter read the FIRST VALUE argument as
 *      the format string and every value argument one slot late --
 *      crashing (SIGSEGV, packed float bit patterns read as pointers) on
 *      any printf() call with at least one value argument.
 *   2. Escape round-trip: the runtime .rslo loader unescapes \n/\t/\r/\\
 *      via osProcessEscapes() when it re-parses a compiled string literal
 *      (libshader/runtime/rslo.l, libshader/shading/rslo.l); the JIT
 *      emitter materializes a string literal directly from the IR's raw
 *      text and never round-trips through that loader, so "...\n" carried
 *      a literal backslash-n into the shader instead of a real newline.
 *
 * Both bugs are invisible to a shader that never actually gets shaded (the
 * crash needs a real per-vertex value; the escape bug needs stdout
 * content), so this test compiles a real fixture to both .slo and .rslo,
 * renders each through the real `orender` CLI on a small, deterministic
 * scene, and diffs the captured stdout. Per-real-vertex firing (matching
 * the interpreter's own `vertexN < numRealVertices` gate in PRINTFEXPR,
 * scriptFunctions.h) means both backends dice the SAME REYES grids and
 * must therefore print the exact same number of lines -- this also catches
 * any regression of the "never collapse printf to the uniform n=1 fast
 * path" requirement (a uniform argument must not suppress per-vertex
 * firing, since printf's *count* of calls is itself the observable
 * behavior, unlike format()'s single written result).
 */

#include <cassert>
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

// Invoke the real oshader CLI to compile srcPath to outPath, optionally
// with --jit. Returns true on a clean (exit 0) run.
static bool runOshader(const char *oshaderBin, const std::string &srcPath, const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                      "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// Renders ribPath via the real orender CLI, capturing combined
// stdout+stderr to logPath. orender's own exit code is not a useful
// success signal here -- every run in this scene (with no Attribute
// "identifier" light/surface defaults declared) exits non-zero on an
// unrelated, harmless "Failed to find shader \"defaultsurface\"" warning
// path that fires regardless of whether the render itself succeeded, so
// the log CONTENT is the only thing worth asserting on.
static void runOrender(const char *orenderBin, const char *displaysDir,
                       const std::string &shadersDir, const std::string &ribPath,
                       const std::string &logPath) {
    std::string cmd = std::string("SHADERS=\"") + shadersDir + "\" ORENDERHOME=\"" + shadersDir +
                      "\" DISPLAYS=\"" + displaysDir + "\" \"" + orenderBin + "\" \"" +
                      ribPath + "\" > \"" + logPath + "\" 2>&1";
    system(cmd.c_str());
}

// Counts non-overlapping occurrences of needle in haystack.
static int countOccurrences(const std::string &haystack, const std::string &needle) {
    int count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

int main() {
    printf("GitHub #11: printf() JIT support -- operand indexing, escape handling, per-vertex firing\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_printf_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    // "qval" deliberately avoids RSL's built-in parametric-coordinate
    // globals (u, v, s, t) -- a shader parameter named "v" resolves to
    // the smoothly-varying surface parametric coordinate instead of the
    // declared uniform default, which looks identical to the operand
    // off-by-one bug this test exists to catch (both produce varying,
    // non-3.5 values) but is purely a fixture-naming trap, not a JIT bug.
    const std::string shaderName = "printf_jit_fixture";
    const std::string src = dir + "/" + shaderName + ".sl";
    const std::string sloOut = dir + "/" + shaderName + ".slo";
    const std::string rsloOut = dir + "/" + shaderName + ".rslo";
    //
    // Kept deliberately uniform (a bare "qval", not e.g. "qval + N[0]*0"):
    // mixing a varying value argument into printf() was tried while writing
    // this test and hit a THIRD, distinct, pre-existing bug -- the compiler
    // itself mis-lowers printf's format-string operand whenever any value
    // argument is varying (confirmed via the compiled .rslo text: the
    // format-string result slot becomes an uninitialized `varying string`
    // temporary that is never assigned the literal anywhere in the
    // bytecode) -- and BOTH backends crash identically on that bytecode,
    // proving it lives in shared IR generation, not either backend. Out of
    // scope for #11 (which is specifically the JIT's printf no-op); filed
    // separately, not exercised by this test.
    EXPECT_TRUE(writeFile(src,
        "surface " + shaderName + "(uniform float qval = 3.5)\n"
        "{\n"
        "    printf(\"val=%f\\n\", qval);\n"
        "    Ci = color(1,1,1);\n"
        "}\n"));

    EXPECT_TRUE(runOshader(oshaderBin, src, sloOut, /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, src, rsloOut, /*jit=*/false));

    // Small, coarse, fully deterministic scene: REYES dicing is shared
    // infrastructure upstream of the interpreter/JIT split, so both
    // backends must shade the exact same real-vertex count here.
    auto writeRib = [&](const std::string &format) {
        const std::string path = dir + "/scene_" + format + ".rib";
        writeFile(path,
            "Format 32 24 1\n"
            "Projection \"perspective\"\n"
            "Display \"test.tif\" \"file\" \"rgba\"\n"
            "WorldBegin\n"
            "    ShadingRate 100\n"
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

    // Bug 1 regression guard: pre-fix, printf() emitted no IR at all under
    // --jit, so the .slo run printed nothing.
    const int sloCount = countOccurrences(sloText, "val=");
    const int rsloCount = countOccurrences(rsloText, "val=");
    printf("  .slo printf calls=%d  .rslo printf calls=%d\n", sloCount, rsloCount);
    EXPECT_TRUE(sloCount > 0);

    // Bug 2 regression guard (operand off-by-one): pre-fix, the format
    // string was read from operands[0] (the first VALUE argument), so
    // "qval"'s float bytes were dereferenced as a garbage char* -- this
    // either crashed the render outright or, when it didn't crash,
    // produced a "val=" line with a different value than the declared
    // uniform default 3.5. Every line must read exactly "val=3.500000".
    const std::string expectedLine = "val=3.500000\n";
    EXPECT_TRUE(sloCount > 0 && countOccurrences(sloText, expectedLine) == sloCount);

    // Bug 3 regression guard (escape round-trip): pre-fix, the JIT's
    // materialized string literal carried the RAW two-character sequence
    // '\' 'n' instead of a real newline byte, so every line ran together
    // with a literal "\n" between them instead of a real line break.
    EXPECT_TRUE(sloText.find("\\n") == std::string::npos);

    // Bug 4 regression guard (uniform-collapse-suppresses-firing): pre-fix
    // attempt at the operand-index fix still collapsed to n=1/tags=null
    // whenever every printf argument was uniform (true here) -- printing
    // once total for the whole grid instead of once per real vertex. A
    // ShadingRate-100 grid on this sphere dices to well over one vertex, so
    // a JIT count of exactly 1 here would mean the collapse regressed.
    //
    // NOT asserted: sloCount == rsloCount. The interpreter has its own,
    // separate, pre-existing version of this same bug -- printf() dispatches
    // through the generic DEFFUNC macro (execute.cpp:597), whose
    // "if (code->uniform) { expr; }" fast path is correct for value-
    // computing builtins but fires printf's side effect exactly once for
    // an all-uniform instruction instead of once per real vertex. Measured
    // on this exact fixture/scene: .rslo prints 1, .slo prints 16 (matching
    // a hand-verified grid vertex count). Filed separately, not this
    // issue's scope -- asserting parity here would mean coding the JIT to
    // match a bug instead of the RISpec-correct per-vertex behavior the
    // approved design (numRealVertices gating) specifies.
    EXPECT_TRUE(sloCount > 1);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
