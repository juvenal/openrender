// Gate-hardening negative test (spec 017-jit-builtin-function-coverage,
// US2, T018, contracts/gate-hardening-contract.md's Verification
// Obligation #2). Shells out to the real `oshader --jit` CLI (not a
// direct emitLLVMBitcode() call) so this exercises the exact end-to-end
// path a developer or `shaders/CMakeLists.txt`'s build step hits: compile
// a fixture .sl that calls a builtin function confirmed to have no JIT
// dispatch case, and assert the hardened coverage gate (llvmEmitter.cpp's
// emitFunction()) fails loudly instead of silently skipping it.
//
// This is deliberately a *process*-level test (fork/exec via std::system),
// mirroring tests/visual/test_visual_render.cpp's own shell-out pattern,
// rather than a link-and-call-emitLLVMBitcode()-directly unit test — the
// contract's own Verification Obligation #2 specifically wants this
// compiled "via oshader --jit", the actual CLI a shader author or
// shaders/CMakeLists.txt's build step would run.

#include <cstdio>
#include <cstdlib>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr, msg)                                                     \
    do {                                                                           \
        if (expr) {                                                                \
            ++g_passed;                                                           \
        }                                                                          \
        else {                                                                     \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__);       \
            ++g_failed;                                                            \
        }                                                                          \
    } while (0)

static bool fileExists(const std::string &path) {
    if (FILE *f = fopen(path.c_str(), "rb")) {
        fclose(f);
        return true;
    }
    return false;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <oshader_path> <fixture_sl_path>\n", argv[0]);
        return 1;
    }
    const char *oshaderPath = argv[1];
    const char *fixturePath = argv[2];

    const std::string outSlo = "gate_hardening_probe.slo";
    // Clean up any stale artifact from a previous run before asserting on
    // its absence below.
    remove(outSlo.c_str());

    std::string cmd = std::string(oshaderPath) + " --jit -o \"" + outSlo +
                       "\" \"" + fixturePath + "\" 2>gate_hardening_stderr.txt";
    int rc = system(cmd.c_str());

    EXPECT_TRUE(rc != 0, "oshader --jit must exit nonzero for an unhandled builtin function");
    EXPECT_TRUE(!fileExists(outSlo), "oshader --jit must not write a .slo file when the gate fires");

    bool diagnosticNamesMnemonic = false;
    if (FILE *f = fopen("gate_hardening_stderr.txt", "rb")) {
        std::string stderrText;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            stderrText.append(buf, n);
        fclose(f);
        diagnosticNamesMnemonic = stderrText.find("degrees") != std::string::npos &&
                                   stderrText.find("coverage gap") != std::string::npos;
        if (!diagnosticNamesMnemonic)
            fprintf(stderr, "stderr was:\n%s\n", stderrText.c_str());
    }
    EXPECT_TRUE(diagnosticNamesMnemonic,
                "stderr must name the specific unhandled mnemonic ('degrees') in a coverage-gap diagnostic");

    remove("gate_hardening_stderr.txt");
    remove(outSlo.c_str());

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
