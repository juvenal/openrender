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
 *
 * Cross-backend parity (sloCount == rsloCount below) originally could not
 * be asserted here: the interpreter had its own separate uniform-collapse
 * bug for printf (GitHub #13, execute.cpp's DEFFUNC macro), fixed
 * afterwards by giving printf its own DEFPRINTFUNC dispatch macro. This
 * test was updated once that landed to assert the parity it always should
 * have had.
 *
 * test_varying_fixture() below covers GitHub #14: a printf() call with a
 * varying value argument crashed both backends identically -- a compiler
 * bug, not either backend's. printf's "o=s.*" prototype has no real
 * return value, so the compiler encodes its format-string argument in the
 * instruction's result slot (irBuilder.cpp's parseLine() generic
 * convention: the first token after any opcode's prototype is always
 * .result). Unlike every other opcode using that slot as a genuine write
 * target, printf only ever READS it. passDCE.cpp's collectLive() didn't
 * know that, so whenever a varying argument forced the uniform format
 * string to be broadcast into a fresh varying temporary (getContainer()'s
 * uniform-to-varying vustring instruction), that broadcast's own result
 * was invisible to the liveness scan and got dead-code-eliminated -- the
 * temporary was left declared but never assigned. Fixed by special-casing
 * printf's result as live in collectLive().
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

// GitHub #11/#13: all-uniform printf() call -- operand indexing, escape
// handling, per-vertex firing, and interpreter/JIT count parity.
static void test_uniform_fixture(const char *oshaderBin, const char *orenderBin, const char *displaysDir) {
    printf("GitHub #11/#13: printf() JIT support -- operand indexing, escape handling, per-vertex firing\n");

    char tmplBuf[] = "/tmp/shading_parity_printf_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return;
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
    // Kept deliberately uniform (a bare "qval") -- the varying-argument
    // case (GitHub #14, a compiler bug: DCE dead-code-eliminated the
    // format string's uniform-to-varying broadcast) is covered by
    // test_varying_fixture() below.
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
    EXPECT_TRUE(rsloCount > 0 && countOccurrences(rsloText, expectedLine) == rsloCount);

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
    EXPECT_TRUE(sloCount > 1);

    // Cross-backend parity (GitHub #13, fixed after this test originally
    // shipped without this assertion): the interpreter had its own,
    // separate, pre-existing version of the JIT's Bug 4 -- printf()
    // dispatched through the generic DEFFUNC macro (execute.cpp), whose
    // "if (code->uniform) { expr; }" fast path is correct for value-
    // computing builtins but fired printf's side effect exactly once for
    // an all-uniform instruction instead of once per real vertex. Fixed by
    // giving printf its own DEFPRINTFUNC macro that never takes that fast
    // path. Both backends now dice the same REYES grids and must print the
    // exact same number of lines.
    EXPECT_TRUE(sloCount == rsloCount);
}

// GitHub #14: printf() call with a varying value argument (forcing the
// format-string result slot to become an allocated-but-unassigned varying
// temporary, pre-fix) -- compiler-level, reproduces identically on both
// backends.
static void test_varying_fixture(const char *oshaderBin, const char *orenderBin, const char *displaysDir) {
    printf("GitHub #14: printf() with a varying value argument -- DCE dropped the format string's broadcast\n");

    char tmplBuf[] = "/tmp/shading_parity_printf_varying_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return;
    const std::string dir = tmpDir;

    // "u" (RSL's built-in parametric surface coordinate) is the simplest
    // guaranteed-varying value available with no arithmetic/indexing of
    // its own -- keeps this test isolated to the format-string-operand
    // bug alone, not any other expression-lowering path.
    const std::string shaderName = "printf_varying_fixture";
    const std::string src = dir + "/" + shaderName + ".sl";
    const std::string sloOut = dir + "/" + shaderName + ".slo";
    const std::string rsloOut = dir + "/" + shaderName + ".rslo";
    EXPECT_TRUE(writeFile(src,
        "surface " + shaderName + "()\n"
        "{\n"
        "    printf(\"u=%f\\n\", u);\n"
        "    Ci = color(1,1,1);\n"
        "}\n"));

    EXPECT_TRUE(runOshader(oshaderBin, src, sloOut, /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, src, rsloOut, /*jit=*/false));

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

    // Pre-fix, both backends crashed (SIGSEGV) rendering this scene, so
    // the log would be truncated mid-render (missing the trailing "u="
    // lines a completed render always has) or empty outright depending on
    // how much stdio buffering flushed before the fault. A non-empty log
    // alone isn't a reliable crash signal here; the per-vertex count and
    // value checks below are.
    EXPECT_TRUE(!sloText.empty());
    EXPECT_TRUE(!rsloText.empty());

    const int sloCount = countOccurrences(sloText, "u=");
    const int rsloCount = countOccurrences(rsloText, "u=");
    printf("  .slo printf calls=%d  .rslo printf calls=%d\n", sloCount, rsloCount);

    // No-crash + correct per-vertex count on both backends is the actual
    // regression guard: pre-fix, this scene never reached the point of
    // printing anything at all.
    EXPECT_TRUE(sloCount > 1);
    EXPECT_TRUE(sloCount == rsloCount);

    // Value correctness: "u" ranges [0,1] across this sphere's grid and
    // must be genuinely non-constant (proving the format string prints
    // real per-vertex data, not e.g. every line reading garbage/"u=0.000000"
    // by coincidence) and identical between backends.
    bool sloVaries = sloText.find("u=0.000000\n") != std::string::npos &&
                     sloText.find("u=1.000000\n") != std::string::npos;
    bool rsloVaries = rsloText.find("u=0.000000\n") != std::string::npos &&
                      rsloText.find("u=1.000000\n") != std::string::npos;
    EXPECT_TRUE(sloVaries);
    EXPECT_TRUE(rsloVaries);

    // Full-log string equality isn't meaningful here -- the two RIB
    // filenames differ (scene_slo.rib vs scene_rslo.rib), which leaks into
    // the "Failed to find shader" warning lines both runs share. Compare
    // just the printf output itself, which is the only thing this test
    // (or #14) is actually about.
    auto extractPrintfLines = [](const std::string &text) {
        std::string out;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t nl = text.find('\n', pos);
            if (nl == std::string::npos)
                nl = text.size();
            if (text.compare(pos, 2, "u=") == 0)
                out.append(text, pos, nl - pos + 1);
            pos = nl + 1;
        }
        return out;
    };
    EXPECT_TRUE(extractPrintfLines(sloText) == extractPrintfLines(rsloText));
}

int main() {
    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    test_uniform_fixture(oshaderBin, orenderBin, displaysDir);
    test_varying_fixture(oshaderBin, orenderBin, displaysDir);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
