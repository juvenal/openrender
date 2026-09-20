// Gate-hardening negative test (spec 017-jit-builtin-function-coverage,
// US2, T018, contracts/gate-hardening-contract.md's Verification
// Obligation #2). Asserts the hardened coverage gate (llvmEmitter.cpp's
// emitFunction(), gated by isHandledOpcode()/kHandledOpcodes[]) fails
// loudly -- nonzero-equivalent (bool false), no .slo written, a
// diagnostic naming the specific mnemonic -- for an opcode absent from
// kHandledOpcodes[], instead of silently skipping it (the exact defect
// class that let random()/urandom() and 26 more builtin functions ship
// broken under --jit with zero diagnostic, before this spec).
//
// REDESIGNED (2026-09-20): originally shelled out to `oshader --jit` on
// a real .sl fixture (fixtures/gate_hardening_probe.sl) calling a real
// RSL builtin confirmed to have no JIT dispatch case yet. That approach
// hit a structural dead end partway through this spec's own US4: once
// every function in kAllFunctionMnemonics (test_opcode_coverage.cpp's
// full builtin-function universe, not just this spec's original 26-
// function inventory) gained a dispatch case, there was no longer any
// real, compilable RSL builtin function left for the fixture to call --
// the fixture had been repointed five times over the course of this
// spec (degrees -> step -> setcomp -> rotate -> concat -> format) as
// each successive target gained coverage, and format() was the last
// one. A permanently-satisfiable negative fixture requires a
// permanently-unhandled builtin, which directly contradicts this spec's
// purpose (closing exactly those gaps) -- so the fixture-based approach
// could never have stayed green past a fully-covered kHandledOpcodes[].
//
// Fixed by driving emitLLVMBitcode() directly with a hand-built
// IRModule containing a synthetic opcode mnemonic
// ("__unhandled_test_opcode__") that can never collide with a real one.
// This tests isHandledOpcode()'s actual contract -- "is this mnemonic
// in kHandledOpcodes[]" -- rather than a proxy for it via a real .sl
// compile, and can never go stale as more functions gain coverage.
//
// A negative-only assertion would be satisfiable by a broken test
// harness (if the hand-built IRModule were malformed in some way that
// made emitLLVMBitcode() fail before ever reaching the coverage gate,
// this test would report "gate fired" when nothing of the sort
// happened) -- so this file asserts a POSITIVE CONTROL first: the same
// IRModule shape, but with only already-handled opcodes ("moveff" then
// "return"), must compile successfully and write a .slo file. Only
// once that passes does the negative assertion (same shape, with one
// synthetic unhandled opcode spliced in) mean anything.

#define LOGGING_IMPLEMENTATION
#include "ir.h"
#include "llvmEmitter.h"
#include "logging.h"
#include "rslo.h" // SLC_xxx constants

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr, msg)                                              \
    do {                                                                    \
        if (expr) {                                                        \
            ++g_passed;                                                    \
        }                                                                   \
        else {                                                              \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
            ++g_failed;                                                    \
        }                                                                   \
    } while (0)

static bool fileExists(const std::string &path) {
    if (FILE *f = fopen(path.c_str(), "rb")) {
        fclose(f);
        return true;
    }
    return false;
}

// True if `path`'s raw bytes contain `needle`. LLVM bitcode keeps external
// function symbol names (like "op_moveff") as readable ASCII substrings
// even though the file is otherwise binary -- confirmed against a real
// compiled .slo before relying on it here. Used to prove the positive
// control's "moveff" instruction actually emitted a call to op_moveff,
// not just that emitLLVMBitcode() returned true for an empty function
// (which it also would for a module containing nothing but "return" --
// true+file-exists alone doesn't distinguish those two cases).
static bool fileContains(const std::string &path, const std::string &needle) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::string data;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        data.append(buf, n);
    fclose(f);
    return data.find(needle) != std::string::npos;
}

// Builds a minimal, otherwise-valid IRModule: one uniform float local
// ("x", cName "temporary_0"), a #!Code section that writes a literal
// into it via "moveff" and terminates with "return". If
// `injectUnhandledOpcode` is set, a synthetic, never-real opcode is
// spliced in between the moveff and the return.
static IRModule buildModule(bool injectUnhandledOpcode) {
    IRModule mod;
    mod.shaderType = "surface";
    mod.version = "1.0.0";

    IRVarInfo x;
    x.cName = "temporary_0";
    x.symbolName = "x";
    x.slcType = SLC_UNIFORM | SLC_FLOAT;
    x.numItems = 1;
    x.defaultValue = "";
    mod.addVar(x);

    IRInstr mv;
    mv.opcode = "moveff";
    mv.result = "temporary_0";
    mv.operands.push_back(IROperand{"1"});
    mod.codeFn.append(mv);

    if (injectUnhandledOpcode) {
        IRInstr bogus;
        bogus.opcode = "__unhandled_test_opcode__";
        mod.codeFn.append(bogus);
    }

    IRInstr ret;
    ret.opcode = "return";
    mod.codeFn.append(ret);

    return mod;
}

// Redirects stderr to `path` for the duration of `fn`, then restores it,
// returning whatever `fn` returned. Used to capture emitLLVMBitcode()'s
// own fprintf(stderr, ...) diagnostic without shelling out to a
// subprocess (this test links libshader_compiler directly).
template <typename Fn>
static auto captureStderr(const std::string &path, Fn &&fn) -> decltype(fn()) {
    fflush(stderr);
    int savedFd = dup(fileno(stderr));
    FILE *redirected = freopen(path.c_str(), "w", stderr);
    (void)redirected;

    auto result = fn();

    fflush(stderr);
    dup2(savedFd, fileno(stderr));
    close(savedFd);
    return result;
}

int main() {
    const std::string positiveSlo = "gate_hardening_positive.slo";
    const std::string negativeSlo = "gate_hardening_negative.slo";
    const std::string stderrPath = "gate_hardening_stderr.txt";
    remove(positiveSlo.c_str());
    remove(negativeSlo.c_str());
    remove(stderrPath.c_str());

    // -----------------------------------------------------------------
    // Positive control: an IRModule built the same way, using only
    // already-handled opcodes, must succeed and write a .slo. This
    // proves the harness itself reaches emitFunction() correctly before
    // the negative assertion below is allowed to mean anything.
    // -----------------------------------------------------------------
    IRModule okMod = buildModule(/*injectUnhandledOpcode=*/false);
    bool okResult = emitLLVMBitcode(okMod, positiveSlo, "gate_positive");
    EXPECT_TRUE(okResult, "emitLLVMBitcode must succeed for an IRModule using only handled opcodes");
    EXPECT_TRUE(fileExists(positiveSlo), "emitLLVMBitcode must write a .slo file when it succeeds");
    EXPECT_TRUE(fileContains(positiveSlo, "op_moveff"),
                "the positive control's moveff instruction must actually emit a call to op_moveff "
                "(not just an empty, trivially-verifying function)");
    remove(positiveSlo.c_str());

    // -----------------------------------------------------------------
    // Negative: the same shape, with one synthetic, guaranteed-never-
    // real opcode spliced in. Must fail, must not write a .slo, and the
    // diagnostic emitFunction() prints must name the mnemonic.
    // -----------------------------------------------------------------
    IRModule badMod = buildModule(/*injectUnhandledOpcode=*/true);
    bool badResult = captureStderr(stderrPath, [&] {
        return emitLLVMBitcode(badMod, negativeSlo, "gate_negative");
    });

    EXPECT_TRUE(!badResult, "emitLLVMBitcode must fail for an IRModule containing an unhandled opcode");
    EXPECT_TRUE(!fileExists(negativeSlo), "emitLLVMBitcode must not write a .slo file when the gate fires");

    bool diagnosticNamesMnemonic = false;
    if (FILE *f = fopen(stderrPath.c_str(), "rb")) {
        std::string stderrText;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            stderrText.append(buf, n);
        fclose(f);
        diagnosticNamesMnemonic = stderrText.find("__unhandled_test_opcode__") != std::string::npos &&
                                   stderrText.find("coverage gap") != std::string::npos;
        if (!diagnosticNamesMnemonic)
            fprintf(stderr, "stderr was:\n%s\n", stderrText.c_str());
    }
    EXPECT_TRUE(diagnosticNamesMnemonic,
                "stderr must name the specific unhandled mnemonic in a coverage-gap diagnostic");

    remove(stderrPath.c_str());
    remove(negativeSlo.c_str());

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
