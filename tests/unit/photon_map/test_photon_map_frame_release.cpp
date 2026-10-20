/**
 * Project: openRender
 *
 * File: test_photon_map_frame_release.cpp
 *
 * Description:
 *   Regression test for GitHub #64: a photon map must outlive every counted
 *   reference to it. CRenderer::render() hands each primitive's CAttributes a
 *   counted reference to its global and caustic photon maps. The world-level
 *   CAttributes release theirs in RiWorldEnd's attributeEnd(), after
 *   CRenderer::endFrame() has already destroyed the frame-file cache. The
 *   cache used to delete the maps outright, so ~CAttributes decremented a
 *   counter in freed memory.
 *
 *   A use-after-free is not observable without a sanitizer, so this test
 *   replaces the global operator delete. For the two photon maps it watches,
 *   delete records the reference count the object still had when it was
 *   freed, fills the object with a poison byte and keeps the memory instead of
 *   freeing it. After RiWorldEnd the test checks that:
 *     - each map was freed exactly once (the hook fired, no double free);
 *     - its reference count was 0 when it was freed (no reference outlived it);
 *     - the poison is intact (nothing wrote into it after it was freed).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <unistd.h>

#include "ri.h"
#include "ri/hiders/photonMap.h"
#include "ri/render/renderer.h"

///////////////////////////////////////////////////////////////////////
// The watched-delete hook. It runs inside every delete in the process, so it
// touches only atomics and preallocated storage: no allocation, no stdio.
namespace {

constexpr int kNumWatched = 2;    // The global and the caustic map
constexpr unsigned char kPoison = 0xA5;

std::atomic<void *> watched[kNumWatched];      // Allocation start of each map
std::atomic<std::size_t> watchedSize[kNumWatched];
std::atomic<int> deletes[kNumWatched];         // Times each map was freed
std::atomic<std::int32_t> refAtFree[kNumWatched];
std::atomic<std::size_t> refOffset{0};         // refCount's offset in a CPhotonMap

// Returns true when p is a watched map and the hook has kept it
bool quarantine(void *p) noexcept {
    if (p == nullptr)
        return false;
    for (int i = 0; i < kNumWatched; i++) {
        if (watched[i].load(std::memory_order_acquire) == p) {
            std::int32_t ref;
            std::memcpy(&ref, static_cast<char *>(p) + refOffset.load(), sizeof(ref));
            refAtFree[i].store(ref);
            deletes[i].fetch_add(1);
            std::memset(p, kPoison, watchedSize[i].load());
            return true;
        }
    }
    return false;
}

void release(void *p) noexcept {
    if (!quarantine(p))
        std::free(p);
}

} // namespace

// Every replaceable deallocation form, so no path to the map's free bypasses
// the hook. The default operator new allocates with malloc (and aligned
// operator new with an aligned malloc), so free() is the matching release.
void operator delete(void *p) noexcept { release(p); }
void operator delete[](void *p) noexcept { release(p); }
void operator delete(void *p, std::size_t) noexcept { release(p); }
void operator delete[](void *p, std::size_t) noexcept { release(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { release(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { release(p); }
void operator delete(void *p, std::align_val_t) noexcept { release(p); }
void operator delete[](void *p, std::align_val_t) noexcept { release(p); }
void operator delete(void *p, std::size_t, std::align_val_t) noexcept { release(p); }
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { release(p); }
void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept { release(p); }
void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept { release(p); }

///////////////////////////////////////////////////////////////////////
// Watch the map the frame-file cache holds under this name
static void watch(int i, const char *name) {
    CPhotonMap *map = CRenderer::getPhotonMap(name);
    void *start = dynamic_cast<void *>(map); // CPhotonMap has four bases
    refOffset.store(static_cast<std::size_t>(reinterpret_cast<char *>(&map->refCount) - static_cast<char *>(start)));
    watchedSize[i].store(sizeof(CPhotonMap));
    watched[i].store(start, std::memory_order_release);
    printf("watching %s map at %p (refCount %d)\n", i == 0 ? "global" : "caustic", start, (int)map->refCount.load());
}

static bool poisonIntact(int i) {
    const unsigned char *bytes = static_cast<const unsigned char *>(watched[i].load());
    const std::size_t size = watchedSize[i].load();
    for (std::size_t b = 0; b < size; b++) {
        if (bytes[b] != kPoison) {
            printf("  byte %zu of the freed map was written after it was freed (0x%02x)%s\n", b, bytes[b],
                   b >= refOffset.load() && b < refOffset.load() + sizeof(std::int32_t) ? ", inside refCount" : "");
            return false;
        }
    }
    return true;
}

int main() {
    char tmpl[] = "/tmp/photon_map_release_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    // Neither file exists: each map starts empty, and with no light nothing is
    // stored, so nothing is written to them either
    const std::string globalName = dir + "/global.gpm";
    const std::string causticName = dir + "/caustic.cpm";
    const char *globalMap = globalName.c_str();
    const char *causticMap = causticName.c_str();

    RtInt emit = 100;
    RtInt numThreads = 1;
    RtPoint square[4] = {{-1, 0, -1}, {-1, 0, 1}, {1, 0, 1}, {1, 0, -1}};

    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &numThreads, RI_NULL);
    RiHider("photon", RI_EMIT, &emit, RI_NULL);
    RiFormat(16, 16, 1);
    RiWorldBegin();
    // World-level attributes: they outlive endFrame() and let go of their map
    // references in RiWorldEnd's attributeEnd()
    RiAttribute(RI_PHOTON, RI_GLOBALMAP, &globalMap, RI_CAUSTICMAP, &causticMap, RI_NULL);
    RiPolygon(4, RI_P, (RtPointer)square, RI_NULL); // render() attaches both maps
    watch(0, globalMap);
    watch(1, causticMap);
    RiWorldEnd();
    RiEnd();

    int failed = 0;
    for (int i = 0; i < kNumWatched; i++) {
        const char *which = i == 0 ? "global" : "caustic";
        const int n = deletes[i].load();
        printf("%s map: freed %d time(s)", which, n);
        if (n != 1) {
            printf(" -- FAILED (expected exactly once%s)\n",
                   n == 0 ? "; either it leaked or the delete hook never saw it" : "");
            failed++;
            continue;
        }
        const std::int32_t ref = refAtFree[i].load();
        printf(", refCount %d at free", (int)ref);
        bool ok = true;
        if (ref != 0) {
            printf(" -- FAILED (freed while %d counted reference(s) remained)", (int)ref);
            ok = false;
        }
        printf("\n");
        if (!poisonIntact(i)) {
            printf("  FAILED (use after free)\n");
            ok = false;
        }
        if (!ok)
            failed++;
    }

    // The quarantined maps are deliberately kept until here
    for (int i = 0; i < kNumWatched; i++) {
        void *p = watched[i].exchange(nullptr);
        if (deletes[i].load() > 0)
            std::free(p);
    }
    unlink(globalMap);
    unlink(causticMap);
    rmdir(dir.c_str());

    printf("\n%s\n", failed == 0 ? "PASSED" : "FAILED");
    return failed == 0 ? 0 : 1;
}
