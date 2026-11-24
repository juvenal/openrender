/*
 * Spec 035, Closure_Arena_* (label closures): where a tree lives and for how long (contracts/closure-abi.md "Building, flattening,
 * lifetime"; FR-002). A tree is built on the batch's threadMemory page, so it is valid until that batch's memEnd; there is no
 * per-node free. closureTreeCopy keeps one longer. The per-batch budget is numRealVertices * 1024 bytes.
 *
 *   Closure_Arena_Release   a tree is unreachable after memEnd: checked on the arena's own mark/release state (memory and
 *                           availableSize return to the mark), not on freed memory
 *   Closure_Arena_Copy      closureTreeCopy survives a release and equals the original node for node
 *   Closure_Arena_Budget    over budget: an EMPTY tree for the affected point, one CODE_LIMIT per frame across 4 threads, nothing
 *                           written past the bytes the builder accounts for (a guard-byte check)
 *   Closure_Arena_Point     setRoot rejects a point outside [0, numRealVertices)
 *
 * Written red first: closureAbi.h, closureRegistry.h and closureTree.h do not exist.
 */
#include <atomic>
#include <cstddef>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "common/memory.h"
#include "closureAbi.h"
#include "closureBuiltins.h"
#include "closureRegistry.h"
#include "closureTree.h"
#include "closures/test_support.h"

using namespace openrender::shading;
using openrender::Spectrum;


struct Seen {
        std::atomic<int> count[(int)ClosureDiagKind::NumKinds];
        std::atomic<int> lastCode[(int)ClosureDiagKind::NumKinds];
        Seen() {
            for (int i = 0; i < (int)ClosureDiagKind::NumKinds; ++i) {
                count[i] = 0;
                lastCode[i] = -1;
            }
        }
};
static void sinkFn(void *context, ClosureDiagKind kind, int code, const char *) {
    Seen *s = (Seen *)context;
    s->count[(int)kind].fetch_add(1);
    s->lastCode[(int)kind].store(code);
}

// a structural comparison of two trees, node for node: ids, weights (bits), parameter blocks (bytes)
static bool sameTree(const OrClosureNode *a, const OrClosureNode *b) {
    if (a == nullptr || b == nullptr)
        return a == b;
    if (a->id != b->id)
        return false;
    if (a->id == OR_CLOSURE_MUL) {
        const OrClosureMul *x = (const OrClosureMul *)a, *y = (const OrClosureMul *)b;
        return std::memcmp(x->weight, y->weight, sizeof(x->weight)) == 0 && sameTree(x->closure, y->closure);
    }
    if (a->id == OR_CLOSURE_ADD) {
        const OrClosureAdd *x = (const OrClosureAdd *)a, *y = (const OrClosureAdd *)b;
        return sameTree(x->closureA, y->closureA) && sameTree(x->closureB, y->closureB);
    }
    const OrClosureComponent *x = (const OrClosureComponent *)a, *y = (const OrClosureComponent *)b;
    const OrClosureEntry *e = closureEntry(a->id);
    return e != nullptr && std::memcmp(x->w, y->w, sizeof(x->w)) == 0 && std::memcmp(x + 1, y + 1, e->blockSize) == 0;
}

static const OrClosureNode *buildSample(ClosureTreeBuilder &b) {
    ClosureDiffuseBlock d = {{0, 0, 1}};
    const float w1[3] = {0.5f, 0.25f, 1}, w2[3] = {2, 1, 0.5f}, half[3] = {0.5f, 0.5f, 0.5f};
    const OrClosureNode *x = b.component(0, w1, &d, sizeof(d));
    const OrClosureNode *y = b.component(5, w2, nullptr, 0);
    return b.mul(half, b.add(x, y));
}

static void sectionRelease() {
    CMemPage *arena;
    memoryInit(arena);
    CMemPage *const firstPage = arena;
    char *const memoryAtStart = arena->memory;
    const int availableAtStart = arena->availableSize;
    {
        memBegin(arena); // the batch's mark
        ClosureTreeBuilder builder(arena, 16, nullptr);
        const OrClosureNode *tree = buildSample(builder);
        CL_CHECK(tree != nullptr);
        CL_CHECK(arena == firstPage);                     // a small tree stays on the page
        CL_CHECK(arena->memory > memoryAtStart);          // the tree is on the arena
        CL_CHECK(arena->availableSize < availableAtStart);
        CL_CHECK(builder.bytesUsed() > 0);
        // the whole of the builder's use is inside the page between the mark and the bump pointer
        CL_CHECK((size_t)(arena->memory - memoryAtStart) >= builder.bytesUsed());
        memEnd(arena); // the batch ends: the tree is released
    }
    CL_CHECK(arena == firstPage);
    CL_CHECK(arena->memory == memoryAtStart);            // the bump pointer is back at the mark
    CL_CHECK_INT(arena->availableSize, availableAtStart); // and so is the free count: nothing of the tree is reachable by the arena
    memoryTini(arena);
}

static void sectionCopy() {
    CMemPage *batch, *keep;
    memoryInit(batch);
    memoryInit(keep);
    const OrClosureNode *copy = nullptr, *original = nullptr;
    ClosureLobeList before;
    {
        memBegin(batch);
        ClosureTreeBuilder builder(batch, 16, nullptr);
        original = buildSample(builder);
        copy = closureTreeCopy(original, keep);
        CL_CHECK(copy != nullptr && copy != original);
        CL_CHECK(sameTree(original, copy)); // node for node, while both exist
        before = closureFlatten(copy, keep);
        // nothing of the copy points into the batch's page
        CL_CHECK(keep != batch);
        CL_CHECK(closureTreeCopy(nullptr, keep) == nullptr);
        memEnd(batch);
    }
    // the batch is released; the copy is intact (it was built on `keep`) and equals what it was
    const ClosureLobeList after = closureFlatten(copy, keep);
    CL_CHECK_INT(after.count, before.count);
    CL_CHECK_INT(after.count, 2);
    for (int i = 0; i < after.count && i < before.count; ++i) {
        CL_CHECK_INT(after.lobes[i].id, before.lobes[i].id);
        for (int c = 0; c < 3; ++c)
            CL_CHECK_BITS(after.lobes[i].weight.c[c], before.lobes[i].weight.c[c]);
    }
    // overwrite the released batch memory: the copy is unaffected
    CMemPage *again = batch;
    void *scribble = ralloc(1024, again);
    std::memset(scribble, 0xEE, 1024);
    const ClosureLobeList third = closureFlatten(copy, keep);
    CL_CHECK_INT(third.count, 2);
    CL_CHECK_BITS(third.lobes[0].weight.c[0], 0.25f); // 0.5 * 0.5
    memoryTini(keep);
    memoryTini(batch);
}

// builds diffuse components until one is refused; returns how many were built
static int buildUntilRefused(ClosureTreeBuilder &builder, const OrClosureNode **last) {
    ClosureDiffuseBlock d = {{0, 0, 1}};
    const float one[3] = {1, 1, 1};
    int built = 0;
    *last = nullptr;
    for (int i = 0; i < 10000; ++i) {
        const OrClosureNode *c = builder.component(0, one, &d, sizeof(d));
        if (c == nullptr)
            break;
        if (*last == nullptr)
            *last = c;
        else if (const OrClosureNode *sum = builder.add(*last, c)) // an add over the budget is refused too
            *last = sum;
        ++built;
    }
    return built;
}

static void sectionBudget() {
    // one batch: 2 points, so a budget of 2 * 1024 = 2048 bytes. A diffuse component is 16 + 12 rounded up to 16 = 32 bytes.
    {
        CMemPage *arena;
        memoryInit(arena);
        CL_CHECK_INT(kClosureTreeBytesPerPoint, 1024);
        Seen seen;
        ClosureFrameDiagnostics diag(sinkFn, &seen);
        ClosureTreeBuilder builder(arena, 2, &diag);
        CL_CHECK_INT(builder.budget(), 2048);
        // guard bytes: everything after the bump pointer of the page is 0xA5 and must still be 0xA5 afterwards, except what the
        // builder accounted for
        CMemPage *const page = arena;
        char *const start = arena->memory;
        const int guardSize = 8192;
        CL_CHECK(arena->availableSize > guardSize + 4096);
        std::memset(start, 0xA5, guardSize);
        const OrClosureNode *last = nullptr;
        // an add node is 24 bytes, so the components are fewer than 64: the loop builds until the first refusal
        buildUntilRefused(builder, &last);
        CL_CHECK(builder.bytesUsed() <= builder.budget());
        CL_CHECK(builder.bytesUsed() > builder.budget() - 64); // it stopped at the budget, not early
        CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BudgetExceeded], 1);
        CL_CHECK_INT(seen.lastCode[(int)ClosureDiagKind::BudgetExceeded], (int)CODE_LIMIT);
        // the page: still the same page, and the bump pointer is within the budget (plus the alignment padding of 16)
        CL_CHECK(arena == page);
        CL_CHECK((size_t)(arena->memory - start) <= builder.budget() + 16);
        // nothing beyond the bump pointer was touched
        bool guardIntact = true;
        for (char *p = arena->memory; p < start + guardSize; ++p)
            if ((unsigned char)*p != 0xA5) {
                guardIntact = false;
                break;
            }
        CL_CHECK(guardIntact);
        // the affected point's tree is EMPTY: the root the batch sets for it after the overflow is nullptr
        builder.setRoot(0, last);
        CL_CHECK(builder.root(0) == nullptr);
        // more components keep failing and do not report again (one per frame)
        ClosureDiffuseBlock d = {{0, 0, 1}};
        const float one[3] = {1, 1, 1};
        for (int i = 0; i < 20; ++i)
            CL_CHECK(builder.component(0, one, &d, sizeof(d)) == nullptr);
        CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BudgetExceeded], 1);
        memoryTini(arena);
    }
    // across 4 threads sharing the frame's diagnostics: exactly one CODE_LIMIT
    {
        Seen seen;
        ClosureFrameDiagnostics diag(sinkFn, &seen);
        std::atomic<int> ready{0};
        std::atomic<int> refused{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t)
            threads.emplace_back([&]() {
                CMemPage *arena;
                memoryInit(arena);
                ClosureTreeBuilder builder(arena, 2, &diag);
                ready.fetch_add(1);
                while (ready.load() < 4) {
                }
                const OrClosureNode *last = nullptr;
                buildUntilRefused(builder, &last);
                builder.setRoot(0, last);
                if (builder.root(0) == nullptr)
                    refused.fetch_add(1);
                memoryTini(arena);
            });
        for (std::thread &th : threads)
            th.join();
        CL_CHECK_INT(refused.load(), 4); // every thread's batch ran over its own budget
        CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BudgetExceeded], 1);
    }
    // under budget: the tree is kept
    {
        CMemPage *arena;
        memoryInit(arena);
        Seen seen;
        ClosureFrameDiagnostics diag(sinkFn, &seen);
        ClosureTreeBuilder builder(arena, 2, &diag);
        const OrClosureNode *tree = buildSample(builder);
        builder.setRoot(1, tree);
        CL_CHECK(builder.root(1) == tree);
        CL_CHECK_INT(seen.count[(int)ClosureDiagKind::BudgetExceeded], 0);
        memoryTini(arena);
    }
}

static void sectionPoint() {
    CMemPage *arena;
    memoryInit(arena);
    Seen seen;
    ClosureFrameDiagnostics diag(sinkFn, &seen);
    ClosureTreeBuilder builder(arena, 3, &diag);
    const OrClosureNode *tree = buildSample(builder);
    for (int p = 0; p < 3; ++p)
        CL_CHECK(builder.root(p) == nullptr); // an empty root is valid and is the start state
    builder.setRoot(1, tree);
    CL_CHECK(builder.root(1) == tree);
    // outside [0, numRealVertices): rejected, reported once per frame, nothing stored
    builder.setRoot(-1, tree);
    builder.setRoot(3, tree);
    builder.setRoot(1 << 20, tree);
    CL_CHECK_INT(seen.count[(int)ClosureDiagKind::PointOutOfRange], 1);
    CL_CHECK(builder.root(-1) == nullptr);
    CL_CHECK(builder.root(3) == nullptr);
    CL_CHECK(builder.root(0) == nullptr);
    CL_CHECK(builder.root(2) == nullptr);
    CL_CHECK(builder.root(1) == tree);
    memoryTini(arena);
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Release") sectionRelease();
    else if (which == "Copy") sectionCopy();
    else if (which == "Budget") sectionBudget();
    else if (which == "Point") sectionPoint();
    else {
        std::printf("usage: test_closure_arena <Release|Copy|Budget|Point>\n");
        return 2;
    }
    return CL_RESULT(("Closure_Arena_" + which).c_str());
}
