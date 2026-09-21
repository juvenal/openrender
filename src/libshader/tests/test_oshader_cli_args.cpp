/**
 * src/libshader/tests/test_oshader_cli_args.cpp
 *
 * Regression tests for GitHub #12: `oshader -I <path>` (space-separated)
 * silently misparsed as a second input file.
 *
 * Root cause (src/oshader/oshader.cpp): the `-I`/`-D` argv matcher only
 * ever read the value from the SAME argv token (`&argv[i][2]`), never
 * advancing past a bare "-I"/"-D" to consume the following token. For the
 * customary space-separated CLI form ("-I path", two argv tokens), this
 * silently registered an EMPTY include path and left the real path token
 * to fall through to the final "else" branch, which pushed it onto
 * sourceFiles as a phantom second input file -- triggering
 * "Output file specified with multiple input files" whenever -o was also
 * given, since only one real .sl was ever passed.
 *
 * Shells out to the real oshader CLI (subprocess, not a linked-in unit
 * test) since the bug lives entirely in main()'s argv loop, not in any
 * separately-callable function.
 */

#include <cstdio>
#include <cstdlib>
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

static bool fileExists(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
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

int main() {
    printf("GitHub #12: oshader -I/-D space-separated argument parsing\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmplBuf[] = "/tmp/oshader_cli_args_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    const std::string incDir = dir + "/includes";
    EXPECT_TRUE(system(("mkdir -p \"" + incDir + "\"").c_str()) == 0);
    EXPECT_TRUE(writeFile(incDir + "/fixture.slh", "#define FIXTURE_CONST 1.0\n"));

    const std::string src = dir + "/fixture.sl";
    EXPECT_TRUE(writeFile(src,
        "#include \"fixture.slh\"\n"
        "surface fixture()\n"
        "{\n"
        "    Ci = color(FIXTURE_CONST, 0, 0);\n"
        "}\n"));

    // Bug 1 regression guard: space-separated "-I <path>" must resolve the
    // include (proving the path was actually consumed, not registered
    // empty) and must NOT misparse the path as a second source file --
    // pre-fix, this failed with "Output file specified with multiple
    // input files" even though there is exactly one real .sl argument.
    {
        const std::string out = dir + "/space_separated.rslo";
        std::string cmd = std::string("\"") + oshaderBin + "\" -I \"" + incDir +
                          "\" -o \"" + out + "\" \"" + src + "\" >\"" + out + ".log\" 2>&1";
        int rc = system(cmd.c_str());
        std::string log = readFile(out + ".log");
        printf("  space-separated -I: rc=%d\n", rc);
        EXPECT_TRUE(rc == 0);
        EXPECT_TRUE(fileExists(out));
        EXPECT_TRUE(log.find("multiple input files") == std::string::npos);
    }

    // Attached form must still work (no regression on the pre-existing,
    // already-working "-Ipath" spelling).
    {
        const std::string out = dir + "/attached.rslo";
        std::string cmd = std::string("\"") + oshaderBin + "\" -I\"" + incDir +
                          "\" -o \"" + out + "\" \"" + src + "\" >\"" + out + ".log\" 2>&1";
        int rc = system(cmd.c_str());
        printf("  attached -Ipath: rc=%d\n", rc);
        EXPECT_TRUE(rc == 0);
        EXPECT_TRUE(fileExists(out));
    }

    // Sibling case: -D has the identical attached-only defect before this
    // fix. A space-separated "-D SYMBOL" preceding "-o <out> <src>" must
    // not misparse SYMBOL as a second source file either.
    {
        const std::string out = dir + "/define_space.rslo";
        std::string cmd = std::string("\"") + oshaderBin + "\" -D DUMMY_DEFINE -I \"" + incDir +
                          "\" -o \"" + out + "\" \"" + src + "\" >\"" + out + ".log\" 2>&1";
        int rc = system(cmd.c_str());
        std::string log = readFile(out + ".log");
        printf("  space-separated -D: rc=%d\n", rc);
        EXPECT_TRUE(rc == 0);
        EXPECT_TRUE(fileExists(out));
        EXPECT_TRUE(log.find("multiple input files") == std::string::npos);
    }

    // A truly bare trailing -I (nothing follows in argv at all) must not
    // crash and must not swallow the real source file -- it should still
    // compile (the fixture's #include will fail without a real -I, which
    // is fine; what matters is oshader doesn't misbehave on the malformed
    // flag itself).
    {
        const std::string out = dir + "/bare_trailing.rslo";
        std::string cmd = std::string("\"") + oshaderBin + "\" -o \"" + out + "\" \"" + src +
                          "\" -I >\"" + out + ".log\" 2>&1";
        int rc = system(cmd.c_str());
        std::string log = readFile(out + ".log");
        printf("  bare trailing -I: rc=%d\n", rc);
        EXPECT_TRUE(log.find("expects a path") != std::string::npos);
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
