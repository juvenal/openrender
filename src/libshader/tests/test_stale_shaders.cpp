/**
 * src/libshader/tests/test_stale_shaders.cpp
 *
 * Spec 039, Phase 4 (US6, research R3.6): a compiled shader from before the
 * call-site change is rejected, never run. The fixtures are generated here from
 * the current compiler, in a temporary directory; none is checked in.
 *
 *   stale_rslo   (T041, LibShader_StaleShaderRejected): a .rslo carrying the
 *                format version of the release before spec 039 (1.0.0) is refused
 *                by the loader, with a diagnostic that names the version, while the
 *                freshly compiled file loads.
 *   stale_slo    a .slo whose call targets are the unversioned `op_random_f` and
 *                `op_random_v` fails at JIT bind time with an unresolved-symbol
 *                diagnostic, while the freshly compiled one binds. The test makes
 *                two things explicit for the ABI change (J1): the compiler now emits
 *                the versioned `_v2` entries and no unversioned one, and the
 *                runtime no longer exports the unversioned names, so a stale
 *                module cannot bind to them.
 *
 * Needs the renderer library (parseShader is its loader) and the JIT engine.
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include "common/diagnostics.h"
#include "llvmJit.h"
#include "ri.h"
#include "shader.h"

// The renderer's shader loader (rendererFiles.cpp).
CShader *parseShader(const char *name, const char *filename);

static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

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

static std::string shaderSource(const char *name) {
    return std::string("surface ") + name + "()\n{\n    Ci = color(random(), random(), 0);\n}\n";
}

static bool compileShaderFile(const std::string &dir, const char *name, bool jit) {
    const char *oshader = getenv("OSHADER_BIN");
    const char *includes = getenv("SHADER_INCLUDES");
    if (!oshader)
        return false;
    spit(dir + "/" + name + ".sl", shaderSource(name));
    std::string cmd = std::string("\"") + oshader + "\"" + (includes ? std::string(" -I") + includes : "") +
                      (jit ? " --jit" : "") + " " + dir + "/" + name + ".sl -o " + dir + "/" + name +
                      (jit ? ".slo" : ".rslo") + " >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

static std::string makeTempDir() {
    char tmpl[] = "/tmp/stale_shaders_XXXXXX";
    return mkdtemp(tmpl);
}

// ---- diagnostics ---------------------------------------------------------

static std::vector<std::string> messages;

static void capture(RtInt, RtInt, const char *message) {
    messages.push_back(message);
}

class CCaptureSink : public CDiagnosticSink {
    public:
        void report(EErrorCode, EDiagnosticSeverity, const char *message) override { messages.push_back(message); }
};

static bool anyMessageContains(const char *text) {
    for (const std::string &m : messages)
        if (m.find(text) != std::string::npos)
            return true;
    return false;
}

// ---- .rslo ---------------------------------------------------------------

static void staleRslo() {
    const std::string dir = makeTempDir();
    EXPECT_TRUE(compileShaderFile(dir, "fresh_probe", false));
    EXPECT_TRUE(compileShaderFile(dir, "stale_probe", false));

    // The stale file is the same shader with the format version of the release
    // before spec 039 in its header.
    std::string text = slurp(dir + "/stale_probe.rslo");
    const size_t at = text.find("#!version");
    EXPECT_TRUE(at != std::string::npos);
    if (at != std::string::npos) {
        const size_t eol = text.find('\n', at);
        text.replace(at, eol - at, "#!version 1.0.0");
        spit(dir + "/stale_probe.rslo", text);
    }

    RiBegin(RI_NULL);
    RiErrorHandler(capture);

    // The freshly compiled shader loads ...
    messages.clear();
    CShader *fresh = parseShader("fresh_probe", (dir + "/fresh_probe.rslo").c_str());
    EXPECT_TRUE(fresh != nullptr);

    // ... and the stale one is refused, with a diagnostic naming the version.
    messages.clear();
    CShader *stale = parseShader("stale_probe", (dir + "/stale_probe.rslo").c_str());
    for (const std::string &m : messages)
        fprintf(stderr, "  diagnostic: %s", m.c_str());
    EXPECT_TRUE(stale == nullptr);
    EXPECT_TRUE(anyMessageContains("ersion"));

    RiEnd();
    for (const char *f : {"fresh_probe.sl", "fresh_probe.rslo", "stale_probe.sl", "stale_probe.rslo"})
        unlink((dir + "/" + f).c_str());
    rmdir(dir.c_str());
}

// ---- .slo ----------------------------------------------------------------

static bool isRandomEntry(const std::string &name, bool versioned) {
    return name == (versioned ? "op_random_f_v2" : "op_random_f") || name == (versioned ? "op_random_v_v2" : "op_random_v");
}

static void staleSlo() {
    const std::string dir = makeTempDir();
    EXPECT_TRUE(compileShaderFile(dir, "fresh_probe", true));
    EXPECT_TRUE(compileShaderFile(dir, "stale_probe", true));

    // Precondition: the emitted module calls the versioned entries and no
    // unversioned one. Rename the versioned calls back to the old names to make
    // the stale module.
    llvm::LLVMContext context;
    auto buffer = llvm::MemoryBuffer::getFile(dir + "/stale_probe.slo");
    EXPECT_TRUE(static_cast<bool>(buffer));
    if (!buffer)
        return;
    auto module = llvm::parseBitcodeFile((*buffer)->getMemBufferRef(), context);
    EXPECT_TRUE(static_cast<bool>(module));
    if (!module)
        return;
    int versionedCalls = 0, unversionedCalls = 0;
    for (llvm::Function &fn : **module)
        for (llvm::BasicBlock &bb : fn)
            for (llvm::Instruction &inst : bb)
                if (auto *call = llvm::dyn_cast<llvm::CallInst>(&inst))
                    if (call->getCalledFunction()) {
                        const std::string name = call->getCalledFunction()->getName().str();
                        versionedCalls += isRandomEntry(name, true);
                        unversionedCalls += isRandomEntry(name, false);
                    }
    fprintf(stderr, "  emitted random() calls: %d versioned (_v2), %d unversioned\n", versionedCalls, unversionedCalls);
    EXPECT_TRUE(versionedCalls == 2);
    EXPECT_TRUE(unversionedCalls == 0);
    if (versionedCalls != 2)
        return; // nothing to rename: the stale half would not mean anything

    for (llvm::Function &fn : **module)
        if (isRandomEntry(fn.getName().str(), true))
            fn.setName(fn.getName().drop_back(3));
    std::error_code ec;
    llvm::raw_fd_ostream out(dir + "/stale_probe.slo", ec);
    llvm::WriteBitcodeToFile(**module, out);
    out.flush();
    out.close();

    CCaptureSink sink;
    installDiagnosticSink(&sink);

    // The freshly compiled module binds ...
    messages.clear();
    CLLVMJitEngine &jit = CLLVMJitEngine::getInstance();
    EXPECT_TRUE(jit.compileShader(dir + "/fresh_probe.slo", "fresh_probe") != nullptr);

    // ... and the stale one does not: the unversioned names are no longer exported.
    messages.clear();
    TShaderJitEntry entry = jit.compileShader(dir + "/stale_probe.slo", "stale_probe");
    for (const std::string &m : messages)
        fprintf(stderr, "  diagnostic: %s", m.c_str());
    EXPECT_TRUE(entry == nullptr);
    EXPECT_TRUE(anyMessageContains("op_random") || anyMessageContains("nresolved") || anyMessageContains("ymbol"));

    for (const char *f : {"fresh_probe.sl", "fresh_probe.slo", "stale_probe.sl", "stale_probe.slo"})
        unlink((dir + "/" + f).c_str());
    rmdir(dir.c_str());
}

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    if (which == "stale_rslo")
        staleRslo();
    else if (which == "stale_slo")
        staleSlo();
    else {
        fprintf(stderr, "usage: %s stale_rslo|stale_slo\n", argv[0]);
        return 2;
    }
    if (g_failed) {
        fprintf(stderr, "FAILED: %d check(s)\n", g_failed);
        return 1;
    }
    return 0;
}
