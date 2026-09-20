#define LOGGING_IMPLEMENTATION
#include "llvmEmitter.h"
#include "logging.h"
#include "opcodes.h"

#include <cstdio>
#include <cstring>

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

// The reachable set is computed, not hand-maintained: every canonical
// mnemonic in kAllOpcodeMnemonics (opcodes.h/.cpp) minus every mnemonic in
// kDeadOpcodes (confirmed structurally unreachable, with evidence — see
// opcodes.cpp) is, by construction, reachable. This means any opcode added
// to opcodes.cpp in the future is automatically part of this test's
// accounting — as a newly-reachable-and-unhandled failure — until it is
// either implemented or added to kDeadOpcodes with evidence. See
// specs/011-jit-opcode-parity/research.md's D3 for the full rationale.
static bool isInSet(const char *const *set, const char *mnemonic) {
    for (int i = 0; set[i] != nullptr; ++i) {
        if (std::strcmp(mnemonic, set[i]) == 0)
            return true;
    }
    return false;
}

// kHandledOpcodes (llvmEmitter.h/.cpp) is consulted directly via extern
// linkage — never re-parsed from source text — so this test always reflects
// emitFunction()'s current dispatch, per contracts/coverage-guard-contract.md.
static bool isHandled(const char *mnemonic) {
    return isInSet(kHandledOpcodes, mnemonic);
}

static void test_reachable_opcodes_are_all_handled() {
    for (int i = 0; kAllOpcodeMnemonics[i] != nullptr; ++i) {
        char mnemonic[32];
        stripOpcodeMnemonic(kAllOpcodeMnemonics[i], mnemonic, sizeof(mnemonic));

        if (isInSet(kDeadOpcodes, mnemonic))
            continue;

        bool handled = isHandled(mnemonic);
        if (!handled) {
            fprintf(stderr,
                    "JIT coverage gap: opcode '%s' is reachable but has no "
                    "emitFunction() case\n",
                    mnemonic);
        }
        EXPECT_TRUE(handled);
    }
}

// FUNCTION_-family builtins (DEFFUNC/DEFLINKFUNC/DEFLIGHTFUNC/DEFSHORTFUNC
// in scriptFunctions.h -> shaderFunctions.h -> giFunctions.h), not
// OPCODE_-family bytecode instructions, so they are structurally outside
// kAllOpcodeMnemonics' coverage above (opcodes.cpp only enumerates
// OPCODE_*). This supersedes issue #1's original narrow, hand-written
// random()/urandom()-only check (spec 017-jit-builtin-function-coverage,
// contracts/function-coverage-guard-contract.md's Supersession note) --
// that check was an explicitly-flagged stopgap pending exactly this
// general extension. Every builtin function reachable from
// kAllFunctionMnemonics is now checked, not just those two, closing the
// same structural gap that let random()/urandom() and 26 more functions
// ship silently broken under --jit with zero test coverage to catch it.
static void test_reachable_functions_are_all_handled() {
    for (int i = 0; kAllFunctionMnemonics[i] != nullptr; ++i) {
        const char *mnemonic = kAllFunctionMnemonics[i];

        // The two DSO/plugin-shadeop dispatcher rows share the literal
        // placeholder text "XXX" -- their real name/prototype is resolved
        // from the loaded plugin at runtime, never a static mnemonic
        // (research.md D7). Never a real callable function; would
        // otherwise be a permanent, unclosable false-positive gap.
        if (std::strcmp(mnemonic, "XXX") == 0)
            continue;

        bool handled = isHandled(mnemonic);
        if (!handled) {
            fprintf(stderr,
                    "JIT coverage gap: builtin function '%s' is reachable but has no "
                    "emitFunction() case\n",
                    mnemonic);
        }
        EXPECT_TRUE(handled);
    }
}

int main() {
    LOG_SET_LEVEL(LOG_LEVEL_NONE);
    test_reachable_opcodes_are_all_handled();
    test_reachable_functions_are_all_handled();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
