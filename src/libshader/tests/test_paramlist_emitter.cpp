// Optional parameter lists in the JIT emitter (GitHub #55).
//
// llvmEmitter.cpp used to emit trace()/transmission()/occlusion()/
// indirectdiffuse() without looking at the (name, value) pairs after their
// positional operands, so a parameter list was dropped without a word. The
// emitter now passes every pair to the runtime, and a list it cannot pass
// (an odd number of operands, a name that is not a string literal, a value
// that resolves to nothing) fails the emit with a diagnostic naming the
// mnemonic instead of being dropped.
//
// Driven the way test_illumination_emitter.cpp drives the illumination forms:
// emitLLVMBitcode() is called directly with hand-built IRModules.
//
//   - Positive control: `trace` and `occlusion` with one resolvable pair
//     compile, write a .slo and call the versioned op_trace_c_v3 /
//     op_occlusion_v3 entries (and no _v2 entry). Only then can the
//     negative cases below mean anything.
//   - `trace` with a trailing name and no value (the shape the compiler
//     still accepts, DEVNOTES_DETAILS/BUGS.md) fails and names 'trace'.
//   - `occlusion` whose value resolves to nothing fails and names 'occlusion'.
//   - `trace` whose name is not a string literal fails and names 'trace'.

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
// substrings (see test_gate_hardening.cpp).
static bool fileContains(const std::string &path, const std::string &needle) {
    return readFile(path).find(needle) != std::string::npos;
}

static void addVar(IRModule &mod, const char *name, int slcType) {
    IRVarInfo v;
    v.cName = name;
    v.symbolName = name;
    v.slcType = slcType;
    v.numItems = 1;
    v.defaultValue = "";
    mod.addVar(v);
}

// A surface module with varying P_local/D_local/N_local (points/vectors),
// a varying color c_res and float f_res, then the single instruction
//   <result> = <mnemonic> (<proto>) <operands...>
// terminated by "return".
static IRModule buildModule(const std::string &mnemonic, const std::string &proto, const std::string &result,
                            const std::vector<std::string> &operands) {
    IRModule mod;
    mod.shaderType = "surface";
    mod.version = "1.0.0";
    addVar(mod, "P_local", SLC_VECTOR | SLC_VPOINT);
    addVar(mod, "D_local", SLC_VECTOR | SLC_VVECTOR);
    addVar(mod, "N_local", SLC_VECTOR | SLC_VNORMAL);
    addVar(mod, "c_res", SLC_VECTOR | SLC_VCOLOR);
    addVar(mod, "f_res", SLC_FLOAT);

    IRInstr ins;
    ins.opcode = mnemonic;
    ins.proto = proto;
    ins.result = result;
    ins.callSite = 0;
    for (const std::string &op : operands)
        ins.operands.push_back(IROperand{op});
    mod.codeFn.append(ins);

    IRInstr ret;
    ret.opcode = "return";
    mod.codeFn.append(ret);
    return mod;
}

// Redirects stderr to `path` for the duration of `fn`, then restores it.
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

static bool emit(IRModule &mod, const std::string &slo, std::string &diagnostic) {
    const std::string stderrPath = slo + ".stderr.txt";
    remove(slo.c_str());
    bool ok = captureStderr(stderrPath, [&] { return emitLLVMBitcode(mod, slo, "paramlist_emitter"); });
    diagnostic = readFile(stderrPath);
    remove(stderrPath.c_str());
    return ok;
}

static void expectRefused(const char *what, IRModule mod, const std::string &slo, const char *mnemonic) {
    std::string diag;
    bool ok = emit(mod, slo, diag);
    EXPECT_TRUE(!ok, what);
    EXPECT_TRUE(!fileExists(slo), "a refused parameter list must not write a .slo");
    const bool named = diag.find(std::string("'") + mnemonic + "'") != std::string::npos;
    EXPECT_TRUE(named, "the diagnostic must name the mnemonic");
    if (!named)
        fprintf(stderr, "stderr was:\n%s\n", diag.c_str());
    remove(slo.c_str());
}

int main() {
    std::string diag;

    // ---- Positive controls
    {
        const std::string slo = "paramlist_trace_positive.slo";
        IRModule mod = buildModule("trace", "c=pvSf", "c_res", {"P_local", "D_local", "\"samples\"", "4"});
        bool ok = emit(mod, slo, diag);
        EXPECT_TRUE(ok, "trace with one resolvable pair must compile");
        if (!ok)
            fprintf(stderr, "stderr was:\n%s\n", diag.c_str());
        EXPECT_TRUE(fileContains(slo, "op_trace_c_v3"), "trace must call op_trace_c_v3");
        EXPECT_TRUE(!fileContains(slo, "op_trace_c_v2"), "trace must no longer call op_trace_c_v2");
        remove(slo.c_str());
    }
    {
        const std::string slo = "paramlist_occlusion_positive.slo";
        IRModule mod = buildModule("occlusion", "f=pnfSf", "f_res", {"P_local", "N_local", "16", "\"maxdist\"", "0.5"});
        bool ok = emit(mod, slo, diag);
        EXPECT_TRUE(ok, "occlusion with one resolvable pair must compile");
        if (!ok)
            fprintf(stderr, "stderr was:\n%s\n", diag.c_str());
        EXPECT_TRUE(fileContains(slo, "op_occlusion_v3"), "occlusion must call op_occlusion_v3");
        EXPECT_TRUE(!fileContains(slo, "op_occlusion_v2"), "occlusion must no longer call op_occlusion_v2");
        remove(slo.c_str());
    }

    // ---- Negative: lists the emitter cannot pass
    expectRefused("trace with a trailing name and no value must fail to compile",
                  buildModule("trace", "c=pvS", "c_res", {"P_local", "D_local", "\"samples\""}),
                  "paramlist_trace_odd.slo", "trace");
    expectRefused("occlusion with an unresolvable value must fail to compile",
                  buildModule("occlusion", "f=pnfSf", "f_res", {"P_local", "N_local", "16", "\"maxdist\"", "__no_such_var__"}),
                  "paramlist_occlusion_unresolved.slo", "occlusion");
    expectRefused("trace whose parameter name is not a string literal must fail to compile",
                  buildModule("trace", "c=pvff", "c_res", {"P_local", "D_local", "4", "4"}),
                  "paramlist_trace_badname.slo", "trace");

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
