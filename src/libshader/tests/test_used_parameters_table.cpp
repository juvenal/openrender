#define LOGGING_IMPLEMENTATION
#include "logging.h"
#include "llvmEmitter.h"
#include "rendererc.h"

#include <cstdio>
#include <cstring>

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr) do { \
    if (expr) { ++g_passed; } \
    else { fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); ++g_failed; } \
} while (0)

// Independent re-expansion of the same interpreter headers llvmEmitter.cpp's
// kOpcodeParamTable is built from (contracts/table-parity-contract.md).
// Both sides #include scriptFunctions.h/scriptOpcodes.h by design — this
// confirms the X-macro mechanism compiles and reproduces the expected
// (text, params) shape, not an independently-sourced comparison (see the
// contract's Non-goals: it cannot see the gating-condition bug this whole
// feature fixes, only whether the table itself matches its own source).
namespace {
struct ExpectedEntry { const char *text; unsigned int params; };
}

#define DEFOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, params) {text, static_cast<unsigned int>(params)},
#define DEFSHORTOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, params) {text, static_cast<unsigned int>(params)},
#define DEFLINKOPCODE(name, text, nargs) {text, 0u},
#define DEFLINKFUNC(name, text, prototype, par) {text, static_cast<unsigned int>(par)},
#define DEFFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFLIGHTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},
#define DEFSHORTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) {text, static_cast<unsigned int>(par)},

static const ExpectedEntry kExpectedTable[] = {
#include "scriptFunctions.h"
#include "scriptOpcodes.h"
    {nullptr, 0u}
};

#undef DEFOPCODE
#undef DEFSHORTOPCODE
#undef DEFLINKOPCODE
#undef DEFLINKFUNC
#undef DEFFUNC
#undef DEFLIGHTFUNC
#undef DEFSHORTFUNC

static void test_table_matches_interpreter_headers() {
    int compilerCount = 0;
    while (kOpcodeParamTable[compilerCount].text != nullptr) ++compilerCount;

    int expectedCount = 0;
    while (kExpectedTable[expectedCount].text != nullptr) ++expectedCount;

    EXPECT_TRUE(compilerCount == expectedCount);
    EXPECT_TRUE(compilerCount > 0);

    int n = compilerCount < expectedCount ? compilerCount : expectedCount;
    for (int i = 0; i < n; ++i) {
        bool textMatches =
            std::strcmp(kOpcodeParamTable[i].text, kExpectedTable[i].text) == 0;
        if (!textMatches) {
            fprintf(stderr,
                "Table-parity mismatch at row %d: compiler text '%s' != "
                "expected text '%s'\n",
                i, kOpcodeParamTable[i].text, kExpectedTable[i].text);
        }
        EXPECT_TRUE(textMatches);

        bool paramsMatch = kOpcodeParamTable[i].params == kExpectedTable[i].params;
        if (!paramsMatch) {
            fprintf(stderr,
                "Table-parity mismatch for '%s': compiler params 0x%08x != "
                "expected params 0x%08x\n",
                kOpcodeParamTable[i].text, kOpcodeParamTable[i].params,
                kExpectedTable[i].params);
        }
        EXPECT_TRUE(paramsMatch);
    }
}

int main() {
    LOG_SET_LEVEL(LOG_LEVEL_NONE);
    test_table_matches_interpreter_headers();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
