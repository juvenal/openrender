/**
 * src/libshader/tests/test_call_sites.cpp
 *
 * Spec 039, Phase 4 (US6, contracts/shading-runtime.md C1-C3): call-site
 * ordinals for the stochastic builtins.
 *
 *   ordinals          (T039, LibShader_CallSiteOrdinals, C1 and C3): one shader with
 *                     two random() calls in a loop, a trace, a gather and an
 *                     illuminance is compiled to .rslo and with --jit to .slo. Every
 *                     stochastic call carries its ordinal (in IR order, from 0): as the
 *                     LAST operand of the instruction in the .rslo text, and as the
 *                     LAST argument, an i32 constant, of the call to the `_v2` entry in
 *                     the .slo bitcode. The two lists, with the kind of each call, must
 *                     be identical. Comparing two in-memory IRModules would prove
 *                     nothing, because both backends emit from the same IR; this reads
 *                     the emitted files.
 *   random_not_pure   (T040, LibShader_RandomNotPure, C2): after the IR passes two
 *                     textually identical random() calls are still two calls, and a
 *                     random() inside a loop stays inside it. A characterization test:
 *                     it may already pass today.
 *
 * The .rslo is produced by the real oshader binary (OSHADER_BIN) and read as text; the
 * IR checks compile in process (emitJIT, the pattern of test_used_parameters_gating).
 */

#define LOGGING_IMPLEMENTATION
#include "logging.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/MemoryBuffer.h>

#include "ir.h"
#include "rslo.h" // CScriptContext

static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

static const char *kShaderSource =
    "surface call_sites_probe()\n"
    "{\n"
    "    float a = 0;\n"
    "    float i;\n"
    "    for (i = 0; i < 3; i += 1) {\n"
    "        a += random();\n"
    "        a += random();\n"
    "    }\n"
    "    color t = trace(P, I);\n"
    "    float h = 0;\n"
    "    gather(\"illuminance\", P, N, 0, 4) { h += 1; }\n"
    "    illuminance(P, N, PI/2) { a += 1; }\n"
    "    Ci = color(a, h, t[0]);\n"
    "}\n";

struct Site {
        std::string kind; // random, trace, gather, illuminance
        long ordinal;
        bool operator<(const Site &o) const { return kind != o.kind ? kind < o.kind : ordinal < o.ordinal; }
        bool operator==(const Site &o) const { return kind == o.kind && ordinal == o.ordinal; }
};

static std::string slurp(const std::string &path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void spit(const std::string &path, const std::string &text) {
    std::ofstream f(path);
    f << text;
}

// The stochastic instructions of the .rslo text: opcode -> kind.
static const std::map<std::string, std::string> kRsloOpcodes = {
    {"random", "random"},
    {"urandom", "random"},
    {"trace", "trace"},
    {"transmission", "trace"},
    {"visibility", "trace"},
    {"gatherHeader", "gather"},
    {"occlusion", "trace"},
    {"indirectdiffuse", "trace"},
    {"illuminance", "illuminance"},
    {"solar", "illuminance"},
    {"illuminate", "illuminance"},
};

// The call-site ordinals in .rslo text: the last operand of each stochastic instruction.
static std::vector<Site> rsloSites(const std::string &text) {
    std::vector<Site> out;
    std::istringstream lines(text);
    std::string line;
    bool inCode = false;
    while (std::getline(lines, line)) {
        if (line.rfind("#!Init:", 0) == 0 || line.rfind("#!Code:", 0) == 0) {
            inCode = true;
            continue;
        }
        if (!inCode)
            continue;
        std::istringstream tok(line);
        std::vector<std::string> t;
        for (std::string w; tok >> w;)
            t.push_back(w);
        if (t.empty())
            continue;
        auto it = kRsloOpcodes.find(t[0]);
        if (it == kRsloOpcodes.end())
            continue;
        char *end = nullptr;
        const long v = strtol(t.back().c_str(), &end, 10);
        // A missing operand shows up as a non-number (or the instruction's own last
        // ordinary operand): record -1 so the lists differ.
        out.push_back({it->second, (end && *end == '\0' && t.size() > 1) ? v : -1});
    }
    return out;
}

static const char *kindOfCallee(const std::string &name) {
    if (name.find("random") != std::string::npos)
        return "random";
    if (name.find("gather") != std::string::npos)
        return "gather";
    if (name.find("illuminance") != std::string::npos || name.find("solar") != std::string::npos ||
        name.find("illuminate") != std::string::npos)
        return "illuminance";
    return "trace";
}

// The call-site ordinals in .slo bitcode: the last argument (an i32 constant) of every
// call to a versioned `_v2` entry.
static bool sloSites(const std::string &path, std::vector<Site> &out, std::vector<std::string> *calleesWithoutV2 = nullptr) {
    llvm::LLVMContext context;
    auto buffer = llvm::MemoryBuffer::getFile(path);
    if (!buffer)
        return false;
    auto module = llvm::parseBitcodeFile((*buffer)->getMemBufferRef(), context);
    if (!module)
        return false;
    for (llvm::Function &fn : **module)
        for (llvm::BasicBlock &bb : fn)
            for (llvm::Instruction &inst : bb) {
                auto *call = llvm::dyn_cast<llvm::CallInst>(&inst);
                if (!call || !call->getCalledFunction())
                    continue;
                const std::string name = call->getCalledFunction()->getName().str();
                if (name.rfind("op_", 0) != 0 && name.rfind("rsl_", 0) != 0)
                    continue;
                const bool versioned = name.size() > 3 && name.compare(name.size() - 3, 3, "_v2") == 0;
                if (!versioned) {
                    if (calleesWithoutV2)
                        calleesWithoutV2->push_back(name);
                    continue;
                }
                long v = -1;
                if (call->arg_size() > 0)
                    if (auto *c = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(call->arg_size() - 1)))
                        v = (long)c->getZExtValue();
                out.push_back({kindOfCallee(name), v});
            }
    return true;
}

static std::string makeTempDir() {
    char tmpl[] = "/tmp/call_sites_XXXXXX";
    return mkdtemp(tmpl);
}

// ---------------------------------------------------------------------------
// T039: LibShader_CallSiteOrdinals (C1, C3)
// ---------------------------------------------------------------------------
static void ordinals() {
    const char *oshader = getenv("OSHADER_BIN");
    const char *includes = getenv("SHADER_INCLUDES");
    EXPECT_TRUE(oshader != nullptr);
    if (!oshader)
        return;
    const std::string dir = makeTempDir();
    spit(dir + "/call_sites_probe.sl", kShaderSource);
    const std::string inc = includes ? std::string(" -I") + includes : "";
    const std::string base = std::string("\"") + oshader + "\"" + inc + " " + dir + "/call_sites_probe.sl";
    EXPECT_TRUE(system((base + " -o " + dir + "/probe.rslo >/dev/null 2>&1").c_str()) == 0);
    EXPECT_TRUE(system((base + " --jit -o " + dir + "/probe.slo >/dev/null 2>&1").c_str()) == 0);

    std::vector<Site> rslo = rsloSites(slurp(dir + "/probe.rslo"));
    std::vector<Site> slo;
    EXPECT_TRUE(sloSites(dir + "/probe.slo", slo));

    // IR order from 0: random, random, trace, gather, illuminance.
    const std::vector<Site> expected = {{"random", 0}, {"random", 1}, {"trace", 2}, {"gather", 3}, {"illuminance", 4}};
    auto show = [](const char *tag, const std::vector<Site> &v) {
        fprintf(stderr, "  %s:", tag);
        for (const Site &s : v)
            fprintf(stderr, " %s=%ld", s.kind.c_str(), s.ordinal);
        fprintf(stderr, "\n");
    };
    show(".rslo", rslo);
    show(".slo ", slo);
    EXPECT_TRUE(rslo == expected);

    // The same calls, with the same ordinals, in the same kinds (order within the
    // bitcode follows the emitter's function layout, so compare sorted).
    std::vector<Site> a = rslo, b = slo;
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    EXPECT_TRUE(a == b);

    for (const char *f : {"probe.rslo", "probe.slo", "call_sites_probe.sl"})
        unlink((dir + "/" + f).c_str());
    rmdir(dir.c_str());
}

// ---------------------------------------------------------------------------
// T040: LibShader_RandomNotPure (C2)
// ---------------------------------------------------------------------------
static std::unique_ptr<IRModule> compileToIR(const std::string &source, const std::string &dir) {
    const std::string sl = dir + "/ir.sl", out = dir + "/ir.slo";
    spit(sl, source);
    CScriptContext ctx;
    ctx.emitJIT = true;
    FILE *in = fopen(sl.c_str(), "r");
    if (!in)
        return nullptr;
    const bool ok = ctx.compile(in, const_cast<char *>(out.c_str())) != 0;
    fclose(in);
    unlink(sl.c_str());
    if (!ok || !ctx.lastCompiledModule)
        return nullptr;
    return std::move(ctx.lastCompiledModule);
}

// The code function's instructions, flattened in order.
static std::vector<const IRInstr *> flatten(const IRModule &m) {
    std::vector<const IRInstr *> v;
    for (const IRBlock &b : m.codeFn.blocks)
        for (const IRInstr &i : b.instrs)
            v.push_back(&i);
    return v;
}

static void randomNotPure() {
    const std::string dir = makeTempDir();

    // Two textually identical random() calls stay two calls.
    {
        auto ir = compileToIR(
            "surface two_randoms()\n{\n    float a = random();\n    float b = random();\n    Ci = color(a, b, 0);\n}\n", dir);
        EXPECT_TRUE(ir != nullptr);
        if (ir) {
            int n = 0;
            for (const IRInstr *i : flatten(*ir))
                n += i->opcode == "random";
            fprintf(stderr, "  two identical random() calls -> %d random instruction(s)\n", n);
            EXPECT_TRUE(n == 2);
        }
    }

    // A random() inside a loop is not hoisted out of it, whether its destination is
    // a varying or a uniform variable.
    for (const char *decl : {"float", "uniform float"}) {
        const std::string src = std::string("surface loop_random()\n{\n    ") + decl +
                                " u = 0;\n    uniform float i;\n    for (i = 0; i < 4; i += 1) {\n        u = random();\n    }\n"
                                "    Ci = color(u, u, u);\n}\n";
        auto ir = compileToIR(src, dir);
        if (!ir) {
            // A uniform destination for random() may not compile at all; that is not
            // what this checks.
            fprintf(stderr, "  (%s destination does not compile; skipped)\n", decl);
            continue;
        }
        long begin = -1, end = -1, rnd = -1, idx = 0;
        int count = 0;
        for (const IRInstr *i : flatten(*ir)) {
            if (i->opcode == "forbegin" && begin < 0)
                begin = idx;
            if (i->opcode == "forend")
                end = idx;
            if (i->opcode == "random") {
                rnd = idx;
                ++count;
            }
            ++idx;
        }
        fprintf(stderr, "  %s destination: random at %ld, loop %ld..%ld, %d random instruction(s)\n", decl, rnd, begin, end,
                count);
        EXPECT_TRUE(count == 1);
        EXPECT_TRUE(begin >= 0 && rnd > begin && rnd < end);
    }
    unlink((dir + "/ir.slo").c_str());
    rmdir(dir.c_str());
}

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    if (which == "ordinals")
        ordinals();
    else if (which == "random_not_pure")
        randomNotPure();
    else {
        fprintf(stderr, "usage: %s ordinals|random_not_pure\n", argv[0]);
        return 2;
    }
    if (g_failed) {
        fprintf(stderr, "FAILED: %d check(s)\n", g_failed);
        return 1;
    }
    return 0;
}
