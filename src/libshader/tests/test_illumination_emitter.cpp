// Illumination operand forms in the JIT emitter (spec
// 024-shading-services-routing, US1, T013, FR-019 and the emitter hardening
// in contracts/jit-op-abi.md).
//
// llvmEmitter.cpp used to emit a begin call for an `illuminate` or `solar`
// instruction only when its operands resolved to a known form, and skipped
// the call silently otherwise. That is how the no-argument `solar()` form
// (research R1) went unnoticed: its only operand is the end label, so no
// begin call was ever emitted.
//
// Driven the way test_gate_hardening.cpp drives the opcode gate: emitLLVMBitcode()
// is called directly with hand-built IRModules, so no real .sl fixture has to
// stay unhandled.
//
//   - Positive control: `illuminate P #!L1 ... endilluminate` with a resolvable
//     P compiles, writes a .slo and calls op_illuminate_begin. Only then can the
//     assertions below mean anything.
//   - Label-only `solar #!L1 ... endsolar` compiles and the bitcode contains
//     op_solar0_begin.
//   - Operands that resolve to none of the four forms
//     (`illuminate __no_such_var__ #!L1`, `solar __no_such_var__ 0 #!L1`) make
//     emitLLVMBitcode() return false, write no .slo, and log a diagnostic that
//     names the mnemonic.

#define LOGGING_IMPLEMENTATION
#include "ir.h"
#include "llvmEmitter.h"
#include "logging.h"
#include "rslo.h" // SLC_xxx constants

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr, msg)                                               \
    do {                                                                     \
        if (expr) {                                                          \
            ++g_passed;                                                      \
        }                                                                    \
        else {                                                               \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
            ++g_failed;                                                      \
        }                                                                    \
    } while (0)

static bool fileExists(const std::string &path) {
    if (FILE *f = fopen(path.c_str(), "rb")) {
        fclose(f);
        return true;
    }
    return false;
}

static std::string readFile(const std::string &path) {
    std::string data;
    if (FILE *f = fopen(path.c_str(), "rb")) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            data.append(buf, n);
        fclose(f);
    }
    return data;
}

// LLVM bitcode keeps external function symbol names as readable ASCII
// substrings (see test_gate_hardening.cpp), so a byte search is enough to
// tell which op_* entry points a module calls.
static bool fileContains(const std::string &path, const std::string &needle) {
    return readFile(path).find(needle) != std::string::npos;
}

// A light shader module: a uniform point "P_local", then one construct
//   <mnemonic> <operands...> #!L1 ... <endMnemonic>
// terminated by "return".
static IRModule buildModule(const std::string &mnemonic, const std::vector<std::string> &operands, const std::string &endMnemonic) {
    IRModule mod;
    mod.shaderType = "light";
    mod.version = "1.0.0";

    IRVarInfo p;
    p.cName = "P_local";
    p.symbolName = "P_local";
    p.slcType = SLC_UNIFORM | SLC_VECTOR | SLC_VPOINT;
    p.numItems = 1;
    p.defaultValue = "";
    mod.addVar(p);

    IRInstr begin;
    begin.opcode = mnemonic;
    for (const std::string &op : operands)
        begin.operands.push_back(IROperand{op});
    begin.operands.push_back(IROperand{"#!L1"});
    mod.codeFn.append(begin);

    IRInstr end;
    end.opcode = endMnemonic;
    mod.codeFn.append(end);

    IRInstr ret;
    ret.opcode = "return";
    mod.codeFn.append(ret);

    return mod;
}

// Redirects stderr to `path` for the duration of `fn` (the emitter reports with
// fprintf(stderr, ...)), then restores it and returns whatever `fn` returned.
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

// Emits `mod` to `slo`. Returns false when the emitter refuses; `diagnostic`
// receives what it wrote to stderr.
static bool emit(IRModule &mod, const std::string &slo, std::string &diagnostic) {
    const std::string stderrPath = slo + ".stderr.txt";
    remove(slo.c_str());
    bool ok = captureStderr(stderrPath, [&] { return emitLLVMBitcode(mod, slo, "illumination_emitter"); });
    diagnostic = readFile(stderrPath);
    remove(stderrPath.c_str());
    return ok;
}

int main() {
    std::string diag;

    // ---- Positive control: illuminate P, a resolvable operand
    {
        const std::string slo = "illumination_positive.slo";
        IRModule mod = buildModule("illuminate", {"P_local"}, "endilluminate");
        bool ok = emit(mod, slo, diag);
        EXPECT_TRUE(ok, "illuminate with a resolvable P must compile");
        EXPECT_TRUE(fileExists(slo), "illuminate with a resolvable P must write a .slo");
        EXPECT_TRUE(fileContains(slo, "op_illuminate_begin"),
                    "illuminate with a resolvable P must call op_illuminate_begin (not an empty function)");
        remove(slo.c_str());
    }

    // ---- Label-only solar: the no-argument solar() form
    {
        const std::string slo = "illumination_solar0.slo";
        IRModule mod = buildModule("solar", {}, "endsolar");
        bool ok = emit(mod, slo, diag);
        EXPECT_TRUE(ok, "solar with only its label must compile");
        EXPECT_TRUE(fileExists(slo), "solar with only its label must write a .slo");
        EXPECT_TRUE(fileContains(slo, "op_solar0_begin"),
                    "solar with only its label must call op_solar0_begin");
        remove(slo.c_str());
    }

    // ---- Negative: operands that resolve to none of the four forms
    {
        const std::string slo = "illumination_bad_illuminate.slo";
        IRModule mod = buildModule("illuminate", {"__no_such_var__"}, "endilluminate");
        bool ok = emit(mod, slo, diag);
        EXPECT_TRUE(!ok, "illuminate with an unresolvable operand must fail to compile");
        EXPECT_TRUE(!fileExists(slo), "a failed illuminate must not write a .slo");
        EXPECT_TRUE(diag.find("illuminate") != std::string::npos,
                    "the diagnostic must name the mnemonic 'illuminate'");
        if (diag.find("illuminate") == std::string::npos)
            fprintf(stderr, "stderr was:\n%s\n", diag.c_str());
        remove(slo.c_str());
    }
    {
        const std::string slo = "illumination_bad_solar.slo";
        IRModule mod = buildModule("solar", {"__no_such_var__", "0"}, "endsolar");
        bool ok = emit(mod, slo, diag);
        EXPECT_TRUE(!ok, "solar with an unresolvable operand must fail to compile");
        EXPECT_TRUE(!fileExists(slo), "a failed solar must not write a .slo");
        EXPECT_TRUE(diag.find("solar") != std::string::npos,
                    "the diagnostic must name the mnemonic 'solar'");
        if (diag.find("solar") == std::string::npos)
            fprintf(stderr, "stderr was:\n%s\n", diag.c_str());
        remove(slo.c_str());
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
