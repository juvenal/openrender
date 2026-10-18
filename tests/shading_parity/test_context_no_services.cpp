/**
 * tests/shading_parity/test_context_no_services.cpp
 *
 * A CShadingContext constructed with no renderer services installed, or with
 * no globalArena checkpoint, must not write through its object-hash pointer
 * (GitHub #53).
 *
 * CShadingContext::CShadingContext allocated traceObjectHash only when
 * `s_defaultServices && globalArena.top`, then filled SHADING_OBJECT_CACHE_SIZE
 * entries through it either way. When the condition was false the pointer was
 * never set, so the fill wrote through whatever bytes the object was built in.
 *
 * How the test makes that deterministic, with no AddressSanitizer and no crash:
 * each context is built with placement new over storage the test controls.
 * Before construction every pointer-sized slot of that storage holds the
 * address of a test-owned sentinel array of SHADING_OBJECT_CACHE_SIZE
 * TObjectHash entries, all with .object == nullptr. Whatever slot
 * traceObjectHash occupies, an unset pointer therefore points at the
 * sentinel. The constructor fills each entry's .object with the context's own
 * address ("impossible data"), so the test counts the sentinel entries that
 * now hold the context's address. Any count above zero means the constructor
 * wrote through a pointer it never set. The constructor is compiled in
 * another library, so the compiler cannot fold the read of the unset member
 * away, and the writes stay inside the sentinel, so the process survives to
 * report.
 *
 *   A. No services and no arena checkpoint: the standalone case.
 *   B. Services installed (a stub) but no arena checkpoint: the guard's
 *      other half.
 *   C. Control: inside RiBegin/RiWorldBegin, where the renderer builds its
 *      contexts. The sentinel stays untouched, and the context takes a state
 *      and is destroyed normally. This passes before and after the fix, which
 *      shows A and B are red for the defect and not for the method.
 *
 * The contexts from A and B never receive a shading state (updateState()
 * needs installed services with real global variables: newState() and
 * freeState() call s_defaultServices unconditionally, a separate issue from
 * #53). They are still destroyed, by an explicit destructor call on the
 * placement-new object, and that must not crash: a context that never received
 * a state (currentShadingState == NULL) must be destroyed cleanly. Both
 * sentinel checks run before either destructor, so the write check is reported
 * for A and B even if a destructor then aborts the process. B is destroyed
 * while its stub is still installed, A with no services at all.
 *
 * One RiBegin/RiEnd session in the whole process (case C), as the other
 * tests in this directory do.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "common/memory.h"
#include "libshader/shading/shading.h"
#include "recording_services.h"
#include "ri/core/ri_config.h" // SHADING_OBJECT_CACHE_SIZE
#include "ri/parse/ri.h"

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
};

// Storage a context is built in, prepared so an unset pointer member points
// at the sentinel
struct SPoisonedContext {
        alignas(CMinimalContext) unsigned char storage[sizeof(CMinimalContext)];
        TObjectHash sentinel[SHADING_OBJECT_CACHE_SIZE];

        CMinimalContext *construct() {
            for (TObjectHash &h : sentinel) {
                h.object = nullptr;
                h.rays = nullptr;
                h.numRays = 0;
                h.next = nullptr;
                h.shadeNext = nullptr;
            }

            TObjectHash *target = sentinel;
            static_assert(sizeof(CMinimalContext) % sizeof(void *) == 0, "storage is a whole number of pointer slots");
            for (size_t off = 0; off < sizeof(storage); off += sizeof(void *))
                memcpy(storage + off, &target, sizeof(void *));

            return new (storage) CMinimalContext(0);
        }

        // The number of sentinel entries the constructor wrote its own address into
        int writesThroughUnsetPointer(const CMinimalContext *ctx) const {
            int n = 0;
            for (const TObjectHash &h : sentinel)
                if (h.object == (const CSurface *)(const CShadingContext *)ctx)
                    n++;
            return n;
        }
};

static void report(const char *label, int writes) {
    printf("  %s: %d of %d sentinel entries written through an unset traceObjectHash\n", label, writes, SHADING_OBJECT_CACHE_SIZE);
}

int main() {
    // Static, so the 512-entry sentinels do not sit on the stack
    static SPoisonedContext caseA, caseB, caseC;

    // A. No services, no arena checkpoint
    printf("Case A: no services installed, globalArena not checkpointed\n");
    EXPECT_TRUE(CShadingContext::defaultServices() == nullptr);
    EXPECT_TRUE(globalArena.top == nullptr);
    CMinimalContext *ctxA = caseA.construct();
    {
        const int writes = caseA.writesThroughUnsetPointer(ctxA);
        report("A", writes);
        EXPECT_TRUE(writes == 0);
        EXPECT_TRUE(ctxA->currentShadingState == nullptr);
    }

    // B. Services installed, but no arena checkpoint
    printf("Case B: stub services installed, globalArena not checkpointed\n");
    {
        CStubServices stub;
        CShadingContext::setDefaultServices(&stub);
        EXPECT_TRUE(globalArena.top == nullptr);

        CMinimalContext *ctxB = caseB.construct();
        const int writes = caseB.writesThroughUnsetPointer(ctxB);
        report("B", writes);
        EXPECT_TRUE(writes == 0);
        EXPECT_TRUE(ctxB->currentShadingState == nullptr);

        // Destroy B (stub still installed), then A (no services). Flush first,
        // so the checks above are visible if a destructor aborts the process.
        printf("Case B: destroying a context that never received a state\n");
        fflush(stdout);
        fflush(stderr);
        ctxB->~CMinimalContext();
        printf("  B: destroyed\n");

        CShadingContext::setDefaultServices(nullptr);
    }
    printf("Case A: destroying a context that never received a state, no services\n");
    fflush(stdout);
    fflush(stderr);
    ctxA->~CMinimalContext();
    printf("  A: destroyed\n");

    // C. The renderer's own path: services registered, arena checkpointed
    printf("Case C (control): inside RiBegin/RiWorldBegin\n");
    RiBegin(RI_NULL);
    RiWorldBegin();
    {
        EXPECT_TRUE(CShadingContext::defaultServices() != nullptr);
        EXPECT_TRUE(globalArena.top != nullptr);

        CMinimalContext *ctx = caseC.construct();
        const int writes = caseC.writesThroughUnsetPointer(ctx);
        report("C", writes);
        EXPECT_TRUE(writes == 0);

        ctx->updateState();
        EXPECT_TRUE(ctx->currentShadingState != nullptr);
        ctx->~CMinimalContext();
    }
    RiWorldEnd();
    RiEnd();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
