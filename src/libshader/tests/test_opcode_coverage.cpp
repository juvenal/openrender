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

// random()/urandom() are FUNCTION_-family builtins (DEFFUNC in
// scriptFunctions.h), not OPCODE_-family bytecode instructions, so they are
// structurally outside kAllOpcodeMnemonics' coverage above (opcodes.cpp only
// enumerates OPCODE_*). Hand-checked here as a targeted regression guard for
// GitHub issue #1 (random() silently no-op'd under the JIT because it was
// simply missing from kHandledOpcodes, with zero test coverage anywhere to
// catch it). See the follow-up GitHub issue for extending this guard to the
// full FUNCTION_ mnemonic set instead of one-off hand-written checks.
static void test_random_builtins_are_handled() {
    EXPECT_TRUE(isHandled("random"));
    EXPECT_TRUE(isHandled("urandom"));
}

int main() {
    LOG_SET_LEVEL(LOG_LEVEL_NONE);
    test_reachable_opcodes_are_all_handled();
    test_random_builtins_are_handled();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
