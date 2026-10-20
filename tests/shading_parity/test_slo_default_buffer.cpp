/**
 * tests/shading_parity/test_slo_default_buffer.cpp
 *
 * GitHub #66: a .slo parameter's default buffer was too small for its type.
 *
 * parseSloShader() (src/ri/render/rendererFiles.cpp) allocated every
 * parameter's default as `new float[numFloats]`, including string parameters,
 * whose slots hold a `char *` each. CProgrammableShaderInstance's constructor
 * (src/libshader/shading/shader.cpp) then copied `numFloats * sizeof(char *)`
 * bytes out of it: 8 bytes from a 4-byte allocation for a scalar string, a
 * heap-buffer-overflow read that AddressSanitizer reported for
 * `string texturename = ""` in ShadingParity_UsedParametersOracle. The copied
 * pointer's upper half came from whatever followed the allocation.
 *
 * This test needs no sanitizer. It replaces the array forms of operator new
 * and delete with a tracker that, while armed around getShader(), records the
 * size of every new[] allocation and fills a canary past its end. It then
 * checks, for both a .slo and a .rslo load of the same fixture:
 *   1. each parameter's default buffer in the CShader holds
 *      numFloats * (sizeof(char *) for strings, sizeof(float) otherwise)
 *      bytes, which is what the instance constructor copies;
 *   2. no string slot copied into the instance carries canary bytes, i.e. the
 *      copy did not read past the parent's buffer.
 * The tracker must see the parent buffers: a lookup miss fails the test
 * rather than passing it, so a tracker that is not interposed cannot go green.
 *
 * `parity` mode compares the defaults each instance holds after getShader()
 * (bind-time init included) between the .slo and .rslo loads. Scalar string
 * defaults never reached a .slo shader: the compiler emits no init code for a
 * constant string default (getAssignment(), expression.cpp), and
 * parseSloShader() never parsed the quoted metadata, so `string label =
 * "hello"` read NULL (or the over-read garbage) under the JIT. String arrays
 * were right, because their init code rewrites the instance's buffer.
 *
 * Usage: test_shading_slo_default_buffer [buffer|parity]
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <pthread.h>
#include <string>
#include <unistd.h>

#include "ri/core/shader.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"

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

///////////////////////////////////////////////////////////////////////
// new[] tracker
//
// Every new[] allocation gets kCanaryBytes of kCanary past its requested end.
// While armed, the pointer and its requested size go into a fixed table (no
// allocation inside operator new). delete[] removes a tracked pointer.
static constexpr size_t kCanaryBytes = 16;
static constexpr unsigned char kCanary = 0xA5;
static constexpr int kTableSize = 1 << 16;

struct TrackedAllocation {
        void *ptr;
        size_t size;
};

static TrackedAllocation g_table[kTableSize];
static bool g_armed = false;
static bool g_overflowed = false;
// A pthread mutex, not std::mutex: delete[] still runs during static
// destruction at exit, after a std::mutex would already be destroyed.
static pthread_mutex_t g_tableMutex = PTHREAD_MUTEX_INITIALIZER;

struct TableLock {
        TableLock() { pthread_mutex_lock(&g_tableMutex); }
        ~TableLock() { pthread_mutex_unlock(&g_tableMutex); }
};

static size_t slotFor(void *p) {
    return (size_t)(((uintptr_t)p >> 4) * 2654435761u) & (kTableSize - 1);
}

static void trackInsert(void *p, size_t n) {
    TableLock lock;
    if (!g_armed)
        return;
    size_t s = slotFor(p);
    for (int i = 0; i < kTableSize; i++, s = (s + 1) & (kTableSize - 1)) {
        if (g_table[s].ptr == nullptr || g_table[s].ptr == p) {
            g_table[s].ptr = p;
            g_table[s].size = n;
            return;
        }
    }
    g_overflowed = true;
}

// Linear probing without tombstones: removal re-inserts the rest of the run.
static void trackRemove(void *p) {
    TableLock lock;
    size_t s = slotFor(p);
    for (int i = 0; i < kTableSize; i++, s = (s + 1) & (kTableSize - 1)) {
        if (g_table[s].ptr == nullptr)
            return;
        if (g_table[s].ptr == p)
            break;
    }
    if (g_table[s].ptr != p)
        return;
    g_table[s].ptr = nullptr;
    for (size_t t = (s + 1) & (kTableSize - 1); g_table[t].ptr != nullptr; t = (t + 1) & (kTableSize - 1)) {
        TrackedAllocation e = g_table[t];
        g_table[t].ptr = nullptr;
        size_t u = slotFor(e.ptr);
        while (g_table[u].ptr != nullptr)
            u = (u + 1) & (kTableSize - 1);
        g_table[u] = e;
    }
}

// Returns the requested size of a tracked allocation, or SIZE_MAX if unknown.
static size_t trackedSize(const void *p) {
    TableLock lock;
    size_t s = slotFor((void *)p);
    for (int i = 0; i < kTableSize; i++, s = (s + 1) & (kTableSize - 1)) {
        if (g_table[s].ptr == nullptr)
            return SIZE_MAX;
        if (g_table[s].ptr == p)
            return g_table[s].size;
    }
    return SIZE_MAX;
}

static void setArmed(bool armed) {
    TableLock lock;
    g_armed = armed;
}

static void *trackedNew(size_t n) {
    unsigned char *p = (unsigned char *)malloc(n + kCanaryBytes);
    if (p == nullptr)
        return nullptr;
    memset(p + n, kCanary, kCanaryBytes);
    trackInsert(p, n);
    return p;
}

static void trackedDelete(void *p) {
    if (p == nullptr)
        return;
    trackRemove(p);
    free(p);
}

void *operator new[](size_t n) {
    void *p = trackedNew(n);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}
void *operator new[](size_t n, const std::nothrow_t &) noexcept { return trackedNew(n); }
void operator delete[](void *p) noexcept { trackedDelete(p); }
void operator delete[](void *p, size_t) noexcept { trackedDelete(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { trackedDelete(p); }

///////////////////////////////////////////////////////////////////////
// Fixture

// Every parameter kind whose default the instance constructor copies: the
// issue's `string texturename = ""`, a non-empty string, a string array, and
// float/color/matrix/float-array controls.
static const char *kParams =
    "    string texturename = \"\";\n"
    "    string label = \"hello\";\n"
    "    string names[3] = {\"a\", \"bb\", \"ccc\"};\n"
    "    float Kd = 0.5;\n"
    "    color tint = color(1, 0.5, 0.25);\n"
    "    matrix xf = 1;\n"
    "    float weights[4] = {1, 2, 3, 4}";

static const char *kBody =
    "    color C = texture(texturename);\n"
    "    float w = weights[0] + weights[3];\n"
    "    if (label == names[1])\n"
    "        w = 0;\n"
    "    point Q = transform(xf, P);\n"
    "    Ci = C * Cs * Kd * tint * w * xcomp(Q);\n"
    "    Oi = Os;\n";

static bool writeFixture(const std::string &path, const std::string &shaderName) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fprintf(f, "surface %s(\n%s\n) {\n%s}\n", shaderName.c_str(), kParams, kBody);
    fclose(f);
    return true;
}

static bool runOshader(const char *oshaderBin, const std::string &srcPath, const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                      "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

static const CVariable *findParameter(const CVariable *list, const char *name) {
    for (; list != nullptr; list = list->next)
        if (strcmp(list->name, name) == 0)
            return list;
    return nullptr;
}

static bool hasCanary(const void *slot) {
    unsigned char bytes[sizeof(char *)];
    memcpy(bytes, slot, sizeof(bytes));
    for (unsigned char b : bytes)
        if (b == kCanary)
            return true;
    return false;
}

// The parameters the fixture declares, with their expected slot counts.
struct ExpectedParameter {
        const char *name;
        EVariableType type;
        int numFloats;
};

static const ExpectedParameter kExpected[] = {
    {"texturename", TYPE_STRING, 1},
    {"label", TYPE_STRING, 1},
    {"names", TYPE_STRING, 3},
    {"Kd", TYPE_FLOAT, 1},
    {"tint", TYPE_COLOR, 3},
    {"xf", TYPE_MATRIX, 16},
    {"weights", TYPE_FLOAT, 4},
};

static void checkInstance(const char *backend, CShaderInstance *instance) {
    CProgrammableShaderInstance *prog = dynamic_cast<CProgrammableShaderInstance *>(instance);
    EXPECT_TRUE(prog != nullptr);
    if (prog == nullptr || prog->parent == nullptr) {
        EXPECT_TRUE(prog != nullptr && prog->parent != nullptr);
        return;
    }

    for (const ExpectedParameter &e : kExpected) {
        const CVariable *parentParam = findParameter(prog->parent->parameters, e.name);
        const CVariable *instanceParam = findParameter(prog->parameters, e.name);
        if (parentParam == nullptr || instanceParam == nullptr) {
            fprintf(stderr, "FAIL: %s: parameter \"%s\" missing\n", backend, e.name);
            ++g_failed;
            continue;
        }

        EXPECT_TRUE(parentParam->type == e.type);
        EXPECT_TRUE(parentParam->numFloats == e.numFloats);

        // 1. The parent's buffer holds what the instance constructor copies.
        const size_t slotBytes = (parentParam->type == TYPE_STRING) ? sizeof(char *) : sizeof(float);
        const size_t needed = (size_t)parentParam->numFloats * slotBytes;
        const size_t allocated = trackedSize(parentParam->defaultValue);
        if (allocated == SIZE_MAX) {
            fprintf(stderr, "FAIL: %s: \"%s\" default buffer %p was not seen by the new[] tracker\n",
                    backend, e.name, parentParam->defaultValue);
            ++g_failed;
        }
        else if (allocated < needed) {
            fprintf(stderr, "FAIL: %s: \"%s\" default buffer holds %zu bytes, the instance copies %zu\n",
                    backend, e.name, allocated, needed);
            ++g_failed;
        }
        else {
            ++g_passed;
        }

        // 2. No string slot in the instance came from past the parent's buffer.
        if (parentParam->type == TYPE_STRING && instanceParam->defaultValue != nullptr) {
            const char *const *slots = (const char *const *)instanceParam->defaultValue;
            for (int i = 0; i < instanceParam->numFloats; i++) {
                if (hasCanary(&slots[i])) {
                    fprintf(stderr, "FAIL: %s: \"%s\"[%d] in the instance is %p, read past the parent's buffer\n",
                            backend, e.name, i, (const void *)slots[i]);
                    ++g_failed;
                }
                else {
                    printf("  %s: \"%s\"[%d] = %s%s%s\n", backend, e.name, i,
                           slots[i] ? "\"" : "", slots[i] ? slots[i] : "NULL", slots[i] ? "\"" : "");
                    ++g_passed;
                }
            }
        }
    }
}

// Compares each fixture parameter's default as the .slo and .rslo instances
// hold it after getShader() (bind-time init included). The .rslo interpreter
// path is the reference.
static void checkParity(CShaderInstance *sloInstance, CShaderInstance *rsloInstance) {
    printf("GitHub #66 (parity): .slo parameter defaults match the .rslo load\n");
    if (sloInstance == nullptr || rsloInstance == nullptr) {
        EXPECT_TRUE(sloInstance != nullptr && rsloInstance != nullptr);
        return;
    }

    for (const ExpectedParameter &e : kExpected) {
        const CVariable *s = findParameter(sloInstance->parameters, e.name);
        const CVariable *r = findParameter(rsloInstance->parameters, e.name);
        if (s == nullptr || r == nullptr || s->defaultValue == nullptr || r->defaultValue == nullptr ||
            s->numFloats != r->numFloats || s->type != r->type) {
            fprintf(stderr, "FAIL: parity: parameter \"%s\" missing or shaped differently\n", e.name);
            ++g_failed;
            continue;
        }

        for (int i = 0; i < s->numFloats; i++) {
            if (s->type == TYPE_STRING) {
                const char *const *ss = (const char *const *)s->defaultValue;
                const char *const *rs = (const char *const *)r->defaultValue;
                if (hasCanary(&ss[i])) {
                    fprintf(stderr, "FAIL: parity: .slo \"%s\"[%d] is %p, read past the parent's buffer\n",
                            e.name, i, (const void *)ss[i]);
                    ++g_failed;
                }
                else if ((ss[i] == nullptr) != (rs[i] == nullptr) ||
                         (ss[i] != nullptr && strcmp(ss[i], rs[i]) != 0)) {
                    fprintf(stderr, "FAIL: parity: \"%s\"[%d] is %s%s%s under .slo, \"%s\" under .rslo\n",
                            e.name, i, ss[i] ? "\"" : "", ss[i] ? ss[i] : "NULL", ss[i] ? "\"" : "",
                            rs[i] ? rs[i] : "NULL");
                    ++g_failed;
                }
                else {
                    ++g_passed;
                }
            }
            else {
                const float sf = ((const float *)s->defaultValue)[i];
                const float rf = ((const float *)r->defaultValue)[i];
                if (sf != rf) {
                    fprintf(stderr, "FAIL: parity: \"%s\"[%d] is %g under .slo, %g under .rslo\n",
                            e.name, i, sf, rf);
                    ++g_failed;
                }
                else {
                    ++g_passed;
                }
            }
        }
    }
}

// Compiles the fixture to `dir`/<name>.slo or .rslo. Returns false on failure.
static bool compileFixture(const char *oshaderBin, const std::string &dir, const std::string &name, bool jit) {
    const std::string src = dir + "/" + name + ".sl";
    const std::string out = dir + "/" + name + (jit ? ".slo" : ".rslo");
    EXPECT_TRUE(writeFixture(src, name));
    const bool compiled = runOshader(oshaderBin, src, out, jit);
    EXPECT_TRUE(compiled);
    return compiled;
}

int main(int argc, char **argv) {
    const char *mode = (argc > 1) ? argv[1] : "buffer";
    const bool parity = strcmp(mode, "parity") == 0;
    if (!parity && strcmp(mode, "buffer") != 0) {
        fprintf(stderr, "usage: %s [buffer|parity]\n", argv[0]);
        return 2;
    }

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    char tmplBuf[] = "/tmp/slo_default_buffer_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!oshaderBin || !tmpDir)
        return 1;

    // Distinct names per backend: getShader() caches shaders process-wide by name.
    const std::string dir = tmpDir;
    const std::string sloName = "default_buffer_slo";
    const std::string rsloName = "default_buffer_rslo";
    if (!compileFixture(oshaderBin, dir, rsloName, /*jit=*/false) ||
        !compileFixture(oshaderBin, dir, sloName, /*jit=*/true))
        return 1;

    // cwd is searched first in the default shader path.
    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    RiBegin(RI_NULL);

    setArmed(true);
    CShaderInstance *rsloInstance = CRenderer::context->getShader(rsloName.c_str(), SL_SURFACE, 0, NULL, NULL);
    CShaderInstance *sloInstance = CRenderer::context->getShader(sloName.c_str(), SL_SURFACE, 0, NULL, NULL);
    setArmed(false);

    EXPECT_TRUE(!g_overflowed);
    EXPECT_TRUE(rsloInstance != nullptr);
    EXPECT_TRUE(sloInstance != nullptr);

    if (parity) {
        checkParity(sloInstance, rsloInstance);
    }
    else {
        printf("GitHub #66: parameter default buffers match what the instance copies\n");
        if (rsloInstance)
            checkInstance(".rslo", rsloInstance);
        if (sloInstance)
            checkInstance(".slo", sloInstance);
    }

    if (rsloInstance)
        rsloInstance->detach();
    if (sloInstance)
        sloInstance->detach();

    RiEnd();

    if (chdir(savedCwd) != 0)
        perror("chdir (restoring working directory)");

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
