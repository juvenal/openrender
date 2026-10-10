// Spec 023 (FR-006): the scratch-memory arena object in common.
//
// Exercises a local CArena (never the process-wide globalArena) through the
// same stack-like operations the renderer uses on CRenderer::globalMemory
// today: init/tini, nested begin/end regions, save/restore checkpoints, page
// growth on a large allocation, 8-byte alignment and rstrdup.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "common/memory.h"

namespace {

int failures = 0;

#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
            failures++;                                          \
        }                                                        \
    } while (0)

struct Mark {
        CMemPage *page;
        char *memory;
        int available;
};

Mark mark(const CArena &arena) {
    return {arena.top, arena.top->memory, arena.top->availableSize};
}

bool same(const Mark &a, const Mark &b) {
    return a.page == b.page && a.memory == b.memory && a.available == b.available;
}

} // namespace

int main() {
    CArena arena;
    CHECK(arena.top == nullptr, "a fresh arena has no page");

    arena.init();
    CHECK(arena.top != nullptr, "init() allocates the first page");
    if (arena.top == nullptr)
        return 1;

    const Mark start = mark(arena);

    // Nested regions restore page, free pointer and available size.
    arenaBegin(arena);
    void *a = ralloc(24, arena);
    CHECK(a != nullptr, "ralloc returns memory");
    const Mark inner = mark(arena);
    arenaBegin(arena);
    (void)ralloc(100, arena);
    arenaEnd(arena);
    CHECK(same(mark(arena), inner), "inner arenaEnd restores the outer region's state");
    arenaEnd(arena);
    CHECK(same(mark(arena), start), "outer arenaEnd restores the initial state");

    // Allocations are 8-byte aligned, whatever the requested size.
    arenaBegin(arena);
    for (int size = 1; size <= 17; size++) {
        void *p = ralloc(size, arena);
        CHECK((reinterpret_cast<uintptr_t>(p) & 7) == 0, "ralloc(%d) is 8-byte aligned", size);
    }
    arenaEnd(arena);

    // rstrdup copies into the arena.
    arenaBegin(arena);
    const char *source = "openRender";
    char *copy = rstrdup(source, arena);
    CHECK(copy != source && strcmp(copy, source) == 0, "rstrdup copies the string");
    arenaEnd(arena);

    // Save/restore checkpoints round-trip.
    TMemCheckpoint checkpoint;
    arenaSave(checkpoint, arena);
    (void)ralloc(64, arena);
    arenaRestore(checkpoint, arena);
    CHECK(same(mark(arena), start), "arenaRestore returns to the saved checkpoint");

    // A request larger than the current page grows a new page; arenaEnd
    // unwinds back to the first page.
    arenaBegin(arena);
    const int big = start.page->totalSize + 4096;
    void *large = ralloc(big, arena);
    CHECK(large != nullptr && arena.top != start.page, "a large allocation moves to a new page");
    CHECK(arena.top->prev == start.page, "the new page is chained after the first");
    arenaEnd(arena);
    CHECK(same(mark(arena), start), "arenaEnd unwinds a page growth");

    arena.tini();
    CHECK(arena.top == nullptr, "tini() releases every page");

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all arena checks passed\n");
    return 0;
}
