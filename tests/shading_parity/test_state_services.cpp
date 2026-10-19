/**
 * tests/shading_parity/test_state_services.cpp
 *
 * Shading states and the renderer services they are built from (GitHub #59).
 *
 * A shading state needs installed services with a consistent global-variable
 * table. The interpreter and the JIT index varying[VARIABLE_P ..
 * VARIABLE_CONSTANTWIDTH] and varying[var->entry] with no bounds check, so a
 * state is only usable when the table holds at least the 29 built-in globals
 * and the grid size is positive. Without that, newState() must fail cleanly: a
 * diagnostic, no state, and updateState() leaves currentShadingState NULL.
 * freeState() must free a state by the shape it was built with, never by the
 * services' current table.
 *
 * Each case runs in its own process (one ctest entry per case, selected by
 * argv[1]), so a crash in one case cannot hide another and each holds at most
 * one RiBegin/RiEnd session.
 *
 *   no-services       T1. updateState() with no services: no state, exactly
 *                     one CODE_NOTSTARTED warning for two calls.
 *   stub              T2. updateState() with CStubServices (no globals, grid
 *                     size 0): no state, a CODE_CONSISTENCY diagnostic only.
 *   table-growth      T3. A global declared with RiDeclare while the
 *                     renderer's contexts hold live states (inside
 *                     WorldBegin), which is what makeGlobalVariable() does: the
 *                     only query for the new entry may come from the new state
 *                     (one per context), never from freeing a state that never
 *                     had it, and each context's vertexMemory must match a
 *                     context built fresh from the grown table.
 *   full              T4. The full-services control: a state has every slot,
 *                     E is zero, and newState()/freeState() leave
 *                     vertexMemory where it was.
 *   string-global     T4 variant. The same pair with a uniform and a varying
 *                     string global declared before WorldBegin.
 *   consistent-table  Control for T2. CTableServices (the 29 built-in globals,
 *                     grid size 16) gives a usable state with no renderer.
 */

#include <atomic>
#include <cstdio>
#include <cstring>

#include "common/diagnostics.h"
#include "libshader/shading/shading.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "table_services.h"

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

// The cheapest concrete context: an empty rendering loop, nothing else
class CMinimalContext : public CShadingContext {
    public:
        explicit CMinimalContext(int thread) : CShadingContext(thread) {}

        void renderingLoop() override {}

        // vertexMemory is protected: read it, for this or any other context,
        // through a pointer to the inherited member
        static int vertexMemoryOf(const CShadingContext *c) { return c->*(&CMinimalContext::vertexMemory); }
        int memory() const { return vertexMemory; }
};

// Counts what it is handed, by code
class CCountingSink : public CDiagnosticSink {
    public:
        std::atomic<int> count{0};
        std::atomic<int> notStartedWarnings{0};
        std::atomic<int> consistency{0};

        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            ++count;
            if (code == CODE_NOTSTARTED && severity == DIAGNOSTIC_WARNING)
                ++notStartedWarnings;
            if (code == CODE_CONSISTENCY)
                ++consistency;
            fprintf(stderr, "  diagnostic (code %d, severity %d): %s", (int)code, (int)severity, message);
        }
};

static void flushAll() {
    fflush(stdout);
    fflush(stderr);
}

// Every slot of the state's varying array is allocated and E is zero for the
// whole grid
static void checkUsableState(const CShadingState *state, int numGlobals, int gridSize) {
    EXPECT_TRUE(state != nullptr);
    if (state == nullptr)
        return;
    int missing = 0;
    for (int j = 0; j < numGlobals; j++)
        if (state->varying[j] == nullptr)
            missing++;
    EXPECT_TRUE(missing == 0);

    const float *E = state->varying[VARIABLE_E];
    int nonZero = 0;
    for (int i = 0; i < gridSize * 3 * 3; i++)
        if (E[i] != 0.0f)
            nonZero++;
    EXPECT_TRUE(nonZero == 0);
}

// newState() then freeState() of a fresh (not recycled) state must leave
// vertexMemory unchanged
static void checkStatePairAccounting(CMinimalContext &ctx, const char *label) {
    const int before = ctx.memory();
    CShadingState *s = ctx.newState();
    EXPECT_TRUE(s != nullptr);
    if (s == nullptr)
        return;
    const int during = ctx.memory();
    ctx.freeState(s);
    const int after = ctx.memory();
    printf("  %s: vertexMemory before %d, with the extra state %d, after freeing it %d\n", label, before, during, after);
    EXPECT_TRUE(during > before);
    EXPECT_TRUE(after == before);
}

// T1
static void caseNoServices() {
    printf("T1: updateState() with no services installed\n");
    EXPECT_TRUE(CShadingContext::defaultServices() == nullptr);

    CCountingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);

    CMinimalContext *ctx = new CMinimalContext(0);
    printf("  calling updateState() twice\n");
    flushAll();
    ctx->updateState();
    ctx->updateState();
    EXPECT_TRUE(ctx->currentShadingState == nullptr);
    EXPECT_TRUE(sink.notStartedWarnings == 1);
    EXPECT_TRUE(sink.count == 1);

    delete ctx;
    installDiagnosticSink(previous);
}

// T2
static void caseStub() {
    printf("T2: updateState() with CStubServices (no globals, grid size 0)\n");
    CStubServices stub;
    CShadingContext::setDefaultServices(&stub);

    CCountingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);

    CMinimalContext *ctx = new CMinimalContext(0);
    ctx->updateState();
    printf("  state %s\n", ctx->currentShadingState ? "created (inconsistent table accepted)" : "refused");
    EXPECT_TRUE(ctx->currentShadingState == nullptr);
    EXPECT_TRUE(sink.consistency >= 1);
    EXPECT_TRUE(sink.count == sink.consistency);

    delete ctx;
    installDiagnosticSink(previous);
    CShadingContext::setDefaultServices(nullptr);
}

// T3
static void caseTableGrowth() {
    printf("T3: a global declared while the renderer's states are live\n");
    RiBegin(RI_NULL);
    RiWorldBegin();

    CRendererServices *original = CShadingContext::defaultServices();
    EXPECT_TRUE(original != nullptr);
    EXPECT_TRUE(CRenderer::contexts != nullptr);
    if (original == nullptr || CRenderer::contexts == nullptr) {
        RiWorldEnd();
        RiEnd();
        return;
    }
    const int numContexts = CRenderer::numThreads;
    const int n0 = original->numGlobalVariables();

    // Lives to the end of the session: newState() stores it in the states
    static CCountingTableServices *wrap = new CCountingTableServices(original);
    CShadingContext::setDefaultServices(wrap);

    wrap->limit = n0;
    wrap->numAtOrPastLimit = 0;
    wrap->armed = true;
    printf("  %d contexts, %d globals; declaring one more\n", numContexts, n0);
    flushAll();
    RiDeclare("t3global", "global varying float");
    wrap->armed = false;

    EXPECT_TRUE(original->numGlobalVariables() == n0 + 1);
    printf("  globalVariable(j >= %d) queries during the declaration: %d (one per context expected: %d)\n", n0, wrap->numAtOrPastLimit, numContexts);
    EXPECT_TRUE(wrap->numAtOrPastLimit == numContexts);

    // Each renderer context holds exactly one state built from the grown
    // table, so its accounting must match a context built fresh from it
    {
        CMinimalContext fresh(0);
        fresh.updateState();
        int mismatched = 0;
        for (int i = 0; i < numContexts; i++) {
            if (CMinimalContext::vertexMemoryOf(CRenderer::contexts[i]) != fresh.memory()) {
                printf("  context %d: vertexMemory %d, fresh context %d\n", i, CMinimalContext::vertexMemoryOf(CRenderer::contexts[i]), fresh.memory());
                mismatched++;
            }
        }
        EXPECT_TRUE(mismatched == 0);
        flushAll();
    }

    CShadingContext::setDefaultServices(original);
    RiWorldEnd();
    RiEnd();
}

// T4 and its string-global variant
static void caseFull(bool withStrings) {
    printf("T4%s: full services\n", withStrings ? " (string globals)" : "");
    RiBegin(RI_NULL);
    if (withStrings) {
        // Before WorldBegin, so no context holds a state yet
        RiDeclare("t4ustring", "global uniform string");
        RiDeclare("t4vstring", "global varying string");
    }
    RiWorldBegin();

    CRendererServices *svc = CShadingContext::defaultServices();
    EXPECT_TRUE(svc != nullptr);
    if (svc != nullptr) {
        CMinimalContext ctx(0);
        ctx.updateState();
        checkUsableState(ctx.currentShadingState, svc->numGlobalVariables(), svc->maxGridSize());
        checkStatePairAccounting(ctx, withStrings ? "T4 strings" : "T4");
        flushAll();
    }

    RiWorldEnd();
    RiEnd();
}

// Control for T2: a consistent stand-alone table
static void caseConsistentTable() {
    printf("Control: updateState() with CTableServices (29 globals, grid size %d)\n", CTableServices::kGridSize);
    CTableServices table;
    EXPECT_TRUE(table.numGlobalVariables() == (int)VARIABLE_CONSTANTWIDTH + 1);
    CShadingContext::setDefaultServices(&table);

    CCountingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CMinimalContext ctx(0);
        ctx.updateState();
        checkUsableState(ctx.currentShadingState, table.numGlobalVariables(), CTableServices::kGridSize);
        checkStatePairAccounting(ctx, "control");
    }
    EXPECT_TRUE(sink.count == 0);
    installDiagnosticSink(previous);
    CShadingContext::setDefaultServices(nullptr);
}

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";

    if (strcmp(which, "no-services") == 0)
        caseNoServices();
    else if (strcmp(which, "stub") == 0)
        caseStub();
    else if (strcmp(which, "table-growth") == 0)
        caseTableGrowth();
    else if (strcmp(which, "full") == 0)
        caseFull(false);
    else if (strcmp(which, "string-global") == 0)
        caseFull(true);
    else if (strcmp(which, "consistent-table") == 0)
        caseConsistentTable();
    else {
        fprintf(stderr, "usage: %s no-services|stub|table-growth|full|string-global|consistent-table\n", argv[0]);
        return 2;
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
