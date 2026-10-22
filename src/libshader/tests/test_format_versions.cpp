/**
 * src/libshader/tests/test_format_versions.cpp
 *
 * Spec 039, Phase 4 follow-up: the shader file formats carry a version with a
 * rule (semver). A file is readable when its MAJOR equals the loader's and its
 * MINOR is not greater than the loader's; the PATCH is never checked. The
 * refusal says what the file's version is, what the loader supports and that the
 * shader must be recompiled with oshader. The fixtures are generated here from
 * the current compiler, in a temporary directory; none is checked in.
 *
 *   .rslo
 *     rslo_old_refused      a file from an older major is refused, and the diagnostic
 *                           names both versions and says to recompile with oshader
 *     rslo_newer_refused    a file from a newer major, and one from the same major with
 *                           a greater minor, are refused the same way
 *     rslo_lower_minor      a file with the same major and a lower minor loads (skipped
 *                           while the supported minor is 0: nothing lower exists)
 *     rslo_current_loads    the file the compiler wrote loads
 *   .slo (metadata node `openrender.slo.format`, three i32 operands)
 *     slo_emitted           oshader --jit writes the node, with the SLO_FORMAT_* numbers
 *     slo_old_refused       a module whose node says 1.0.0 is refused BEFORE linking: it
 *                           is otherwise a valid, linkable module, so only the format
 *                           check can refuse it
 *     slo_missing_refused   a module without the node counts as 1.0.0 and is refused
 *     slo_newer_refused     a newer major and a greater minor are refused
 *     slo_current_loads     the module the compiler wrote loads
 *
 * (The stale-call half, a module of the current format whose calls no longer
 * resolve, is test_stale_shaders.cpp.)
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
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include "common/diagnostics.h"
#include "common/rsloFormat.h"
#include "common/sloFormat.h"
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
    char tmpl[] = "/tmp/format_versions_XXXXXX";
    return mkdtemp(tmpl);
}

static void removeDir(const std::string &dir, std::initializer_list<std::string> files) {
    for (const std::string &f : files)
        unlink((dir + "/" + f).c_str());
    rmdir(dir.c_str());
}

static std::string versionString(int major, int minor, int patch) {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
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

static bool anyMessageContains(const std::string &text) {
    for (const std::string &m : messages)
        if (m.find(text) != std::string::npos)
            return true;
    return false;
}

static void showMessages() {
    for (const std::string &m : messages)
        fprintf(stderr, "  diagnostic: %s%s", m.c_str(), (!m.empty() && m.back() == '\n') ? "" : "\n");
}

// The refusal diagnostic names the file's version, the supported one and the
// remedy.
static void expectRefusalText(const std::string &fileVersion, const std::string &supported, const char *remedy) {
    EXPECT_TRUE(anyMessageContains(fileVersion));
    EXPECT_TRUE(anyMessageContains(supported));
    EXPECT_TRUE(anyMessageContains(remedy));
}

// ---- .rslo ---------------------------------------------------------------

static const std::string rsloSupported = versionString(RSLO_FORMAT_MAJOR, RSLO_FORMAT_MINOR, RSLO_FORMAT_PATCH);

// Compile a probe shader to dir/name.rslo, with the given version in its header
// (nothing is rewritten when the version is empty).
static bool makeRslo(const std::string &dir, const char *name, const std::string &version) {
    if (!compileShaderFile(dir, name, false))
        return false;
    if (version.empty())
        return true;
    std::string text = slurp(dir + "/" + name + ".rslo");
    const size_t at = text.find("#!version");
    if (at == std::string::npos)
        return false;
    text.replace(at, text.find('\n', at) - at, "#!version " + version);
    spit(dir + "/" + name + ".rslo", text);
    return true;
}

static CShader *loadRslo(const std::string &dir, const char *name) {
    messages.clear();
    CShader *s = parseShader(name, (dir + "/" + name + ".rslo").c_str());
    showMessages();
    return s;
}

static void rsloRefused(const char *name, const std::string &version) {
    const std::string dir = makeTempDir();
    EXPECT_TRUE(makeRslo(dir, name, version));
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    EXPECT_TRUE(loadRslo(dir, name) == nullptr);
    expectRefusalText(version, rsloSupported, "oshader");
    RiEnd();
    removeDir(dir, {std::string(name) + ".sl", std::string(name) + ".rslo"});
}

// The fixtures use patch 7: the patch is never checked, and a version string that
// is not also the renderer's own (v1.0.0 appears in the loader's message) cannot
// match by accident.
static void rsloOldRefused() {
    rsloRefused("old_probe", versionString(RSLO_FORMAT_MAJOR - 1, 0, 7));
}

static void rsloNewerRefused() {
    rsloRefused("newer_major_probe", versionString(RSLO_FORMAT_MAJOR + 1, 0, 7));
    rsloRefused("newer_minor_probe", versionString(RSLO_FORMAT_MAJOR, RSLO_FORMAT_MINOR + 1, 7));
}

static int rsloLowerMinor() {
    if (RSLO_FORMAT_MINOR == 0) {
        fprintf(stderr, "SKIP: the supported .rslo minor is 0, so no lower minor exists\n");
        return 77;
    }
    const std::string dir = makeTempDir();
    EXPECT_TRUE(makeRslo(dir, "lower_probe", versionString(RSLO_FORMAT_MAJOR, RSLO_FORMAT_MINOR - 1, 7)));
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    EXPECT_TRUE(loadRslo(dir, "lower_probe") != nullptr);
    RiEnd();
    removeDir(dir, {"lower_probe.sl", "lower_probe.rslo"});
    return 0;
}

static void rsloCurrentLoads() {
    const std::string dir = makeTempDir();
    EXPECT_TRUE(makeRslo(dir, "current_probe", ""));
    // The compiler wrote the supported version.
    EXPECT_TRUE(slurp(dir + "/current_probe.rslo").find("#!version " + rsloSupported + "\n") != std::string::npos);
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    EXPECT_TRUE(loadRslo(dir, "current_probe") != nullptr);
    RiEnd();
    removeDir(dir, {"current_probe.sl", "current_probe.rslo"});
}

// ---- .slo ----------------------------------------------------------------

static const char *kFormatKey = "openrender.slo.format";
static const std::string sloSupported = versionString(SLO_FORMAT_MAJOR, SLO_FORMAT_MINOR, SLO_FORMAT_PATCH);

// Rewrite the format node of dir/name.slo: with the three numbers, or removed
// when `version` is null. The module is otherwise untouched (and still links).
static bool rewriteSloFormat(const std::string &dir, const char *name, const int *version) {
    llvm::LLVMContext context;
    auto buffer = llvm::MemoryBuffer::getFile(dir + "/" + name + ".slo");
    if (!buffer)
        return false;
    auto module = llvm::parseBitcodeFile((*buffer)->getMemBufferRef(), context);
    if (!module)
        return false;
    if (llvm::NamedMDNode *old = (*module)->getNamedMetadata(kFormatKey))
        (*module)->eraseNamedMetadata(old);
    if (version) {
        llvm::Type *i32 = llvm::Type::getInt32Ty(context);
        llvm::Metadata *ops[3];
        for (int i = 0; i < 3; ++i)
            ops[i] = llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(i32, version[i]));
        (*module)->getOrInsertNamedMetadata(kFormatKey)->addOperand(llvm::MDNode::get(context, ops));
    }
    std::error_code ec;
    llvm::raw_fd_ostream out(dir + "/" + name + ".slo", ec);
    llvm::WriteBitcodeToFile(**module, out);
    out.flush();
    out.close();
    return !ec;
}

static TShaderJitEntry loadSlo(const std::string &dir, const char *name) {
    messages.clear();
    TShaderJitEntry entry = CLLVMJitEngine::getInstance().compileShader(dir + "/" + name + ".slo", name);
    showMessages();
    return entry;
}

static void sloRefused(const char *name, const int *version, const std::string &fileVersion) {
    const std::string dir = makeTempDir();
    EXPECT_TRUE(compileShaderFile(dir, name, true));
    EXPECT_TRUE(rewriteSloFormat(dir, name, version));
    CCaptureSink sink;
    installDiagnosticSink(&sink);
    EXPECT_TRUE(loadSlo(dir, name) == nullptr);
    expectRefusalText(fileVersion, sloSupported, "oshader --jit");
    // Refused by the format check, not by a failed link: the module itself
    // resolves (the current-format control below loads), so no unresolved symbol
    // may be what the refusal says.
    EXPECT_TRUE(!anyMessageContains("Symbols not found"));
    removeDir(dir, {std::string(name) + ".sl", std::string(name) + ".slo"});
}

static void sloEmitted() {
    const std::string dir = makeTempDir();
    EXPECT_TRUE(compileShaderFile(dir, "emitted_probe", true));
    llvm::LLVMContext context;
    auto buffer = llvm::MemoryBuffer::getFile(dir + "/emitted_probe.slo");
    EXPECT_TRUE(static_cast<bool>(buffer));
    if (buffer) {
        auto module = llvm::parseBitcodeFile((*buffer)->getMemBufferRef(), context);
        EXPECT_TRUE(static_cast<bool>(module));
        if (module) {
            const llvm::NamedMDNode *node = (*module)->getNamedMetadata(kFormatKey);
            fprintf(stderr, "  %s node: %s\n", kFormatKey, node ? "present" : "absent");
            EXPECT_TRUE(node != nullptr);
            if (node) {
                EXPECT_TRUE(node->getNumOperands() == 1);
                const int expected[3] = {SLO_FORMAT_MAJOR, SLO_FORMAT_MINOR, SLO_FORMAT_PATCH};
                if (node->getNumOperands() == 1 && node->getOperand(0)->getNumOperands() == 3)
                    for (int i = 0; i < 3; ++i) {
                        auto *c = llvm::mdconst::dyn_extract<llvm::ConstantInt>(node->getOperand(0)->getOperand(i));
                        EXPECT_TRUE(c && c->getSExtValue() == expected[i]);
                    }
                else
                    EXPECT_TRUE(false); // three integer operands
            }
        }
    }
    removeDir(dir, {"emitted_probe.sl", "emitted_probe.slo"});
}

static void sloOldRefused() {
    const int v[3] = {1, 0, 7};
    sloRefused("old_slo_probe", v, "1.0.7");
}

static void sloMissingRefused() {
    // No key counts as 1.0.0.
    sloRefused("missing_slo_probe", nullptr, "1.0.0");
}

static void sloNewerRefused() {
    const int major[3] = {SLO_FORMAT_MAJOR + 1, 0, 7};
    sloRefused("newer_major_slo_probe", major, versionString(major[0], major[1], major[2]));
    const int minor[3] = {SLO_FORMAT_MAJOR, SLO_FORMAT_MINOR + 1, 7};
    sloRefused("newer_minor_slo_probe", minor, versionString(minor[0], minor[1], minor[2]));
}

static void sloCurrentLoads() {
    const std::string dir = makeTempDir();
    EXPECT_TRUE(compileShaderFile(dir, "current_slo_probe", true));
    CCaptureSink sink;
    installDiagnosticSink(&sink);
    EXPECT_TRUE(loadSlo(dir, "current_slo_probe") != nullptr);
    EXPECT_TRUE(messages.empty());
    removeDir(dir, {"current_slo_probe.sl", "current_slo_probe.slo"});
}

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    int skip = 0;
    if (which == "rslo_old_refused")
        rsloOldRefused();
    else if (which == "rslo_newer_refused")
        rsloNewerRefused();
    else if (which == "rslo_lower_minor")
        skip = rsloLowerMinor();
    else if (which == "rslo_current_loads")
        rsloCurrentLoads();
    else if (which == "slo_emitted")
        sloEmitted();
    else if (which == "slo_old_refused")
        sloOldRefused();
    else if (which == "slo_missing_refused")
        sloMissingRefused();
    else if (which == "slo_newer_refused")
        sloNewerRefused();
    else if (which == "slo_current_loads")
        sloCurrentLoads();
    else {
        fprintf(stderr, "usage: %s rslo_old_refused|rslo_newer_refused|rslo_lower_minor|rslo_current_loads|"
                        "slo_emitted|slo_old_refused|slo_missing_refused|slo_newer_refused|slo_current_loads\n",
                argv[0]);
        return 2;
    }
    if (g_failed) {
        fprintf(stderr, "FAILED: %d check(s)\n", g_failed);
        return 1;
    }
    return skip;
}
