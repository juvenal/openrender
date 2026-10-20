/**
 * Project: openRender
 *
 * File: test_photon_map_sessions.cpp
 *
 * Description:
 *   GitHub #64, the paths beyond a single frame. A photon map is shared by the
 *   frame-file cache and by every CAttributes that render() gave it to, and it
 *   must be freed by the last of them, exactly once, and never written after.
 *
 *   two-frame   One RiBegin/RiEnd session: frame 1 is a photon pass that
 *               stores photons and writes the map, frame 2 a raytrace frame
 *               that reads it back. Each frame loads its own map object.
 *   objectdef   An object definition made inside the world shares the
 *               attributes a world primitive gives the map to. The definition
 *               lives until RiEnd, so those attributes, and the map, must
 *               survive RiWorldEnd and be released at RiEnd.
 *
 *   As in test_photon_map_frame_release.cpp, operator delete is replaced:
 *   for a watched map it records the reference count at free, poisons the
 *   object and keeps the memory, so a use after free is seen without a
 *   sanitizer.
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
#include <sys/stat.h>
#include <unistd.h>

#include "ri.h"
#include "ri/hiders/photonMap.h"
#include "ri/render/renderer.h"

///////////////////////////////////////////////////////////////////////
// The watched-delete hook: atomics and preallocated storage only
namespace {

constexpr int kMaxWatched = 4;
constexpr unsigned char kPoison = 0xA5;

std::atomic<void *> watched[kMaxWatched];
std::atomic<int> deletes[kMaxWatched];
std::atomic<std::int32_t> refAtFree[kMaxWatched];
std::atomic<std::size_t> refOffset{0};

bool quarantine(void *p) noexcept {
    if (p == nullptr)
        return false;
    for (int i = 0; i < kMaxWatched; i++) {
        if (watched[i].load(std::memory_order_acquire) == p) {
            std::int32_t ref;
            std::memcpy(&ref, static_cast<char *>(p) + refOffset.load(), sizeof(ref));
            refAtFree[i].store(ref);
            deletes[i].fetch_add(1);
            std::memset(p, kPoison, sizeof(CPhotonMap));
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
static int numWatched = 0;
static int failed = 0;

static void fail(const char *what) {
    printf("  FAILED: %s\n", what);
    failed++;
}

// Watch the map the current frame's cache holds under this name; returns it
static CPhotonMap *watch(const char *label, const char *name) {
    CPhotonMap *map = CRenderer::getPhotonMap(name);
    void *start = dynamic_cast<void *>(map); // CPhotonMap has four bases
    refOffset.store(static_cast<std::size_t>(reinterpret_cast<char *>(&map->refCount) - static_cast<char *>(start)));
    watched[numWatched].store(start, std::memory_order_release);
    printf("  watching %s map at %p (refCount %d, %d photons)\n", label, start, (int)map->refCount.load(), map->numItems);
    numWatched++;
    return map;
}

// Each watched map was freed once, with no reference left, and not written after
static void checkReleased(const char *when) {
    printf("  %s:\n", when);
    for (int i = 0; i < numWatched; i++) {
        const int n = deletes[i].load();
        printf("    map %d: freed %d time(s)", i, n);
        if (n != 1) {
            printf("\n");
            fail(n == 0 ? "map not freed (leaked, or the delete hook never saw it)" : "map freed more than once");
            continue;
        }
        const std::int32_t ref = refAtFree[i].load();
        printf(", refCount %d at free\n", (int)ref);
        if (ref != 0)
            fail("map freed while counted references remained");
        const unsigned char *bytes = static_cast<const unsigned char *>(watched[i].load());
        for (std::size_t b = 0; b < sizeof(CPhotonMap); b++) {
            if (bytes[b] != kPoison) {
                printf("    byte %zu written after free (0x%02x)\n", b, bytes[b]);
                fail("use after free");
                break;
            }
        }
    }
}

static void resetWatch() {
    for (int i = 0; i < numWatched; i++) {
        void *p = watched[i].exchange(nullptr);
        if (deletes[i].load() > 0)
            std::free(p);
        deletes[i].store(0);
        refAtFree[i].store(0);
    }
    numWatched = 0;
}

static RtPoint floorQuad[4] = {{-2, 0, -2}, {-2, 0, 2}, {2, 0, 2}, {2, 0, -2}};

///////////////////////////////////////////////////////////////////////
// Frame 1 writes the map, frame 2 reads it, one session
static void twoFrame(const std::string &dir) {
    printf("two-frame session\n");
    const std::string mapName = dir + "/two-frame.gpm";
    const std::string imageName = dir + "/two-frame.tif";
    const char *map = mapName.c_str();
    RtInt numThreads = 1;
    RtInt emit = 2000;
    RtFloat intensity = 20;
    RtPoint from = {0, 2, 0};

    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &numThreads, RI_NULL);

    // Frame 1: photon pass
    RiFrameBegin(1);
    RiHider("photon", RI_EMIT, &emit, RI_NULL);
    RiFormat(16, 16, 1);
    RiTranslate(0, -1, 5);
    RiWorldBegin();
    RiLightSource("pointlight", RI_INTENSITY, &intensity, RI_FROM, from, RI_NULL);
    RiAttribute(RI_PHOTON, RI_GLOBALMAP, &map, RI_NULL);
    RiPolygon(4, RI_P, (RtPointer)floorQuad, RI_NULL);
    watch("frame 1", map);
    RiWorldEnd();
    RiFrameEnd();

    struct stat st;
    if (stat(map, &st) != 0 || st.st_size == 0)
        fail("frame 1 wrote no photon map");
    else
        printf("  frame 1 wrote %lld bytes\n", (long long)st.st_size);

    // Frame 2: render with the map frame 1 wrote
    RiFrameBegin(2);
    RiHider("raytrace", RI_NULL);
    RiDisplay((RtToken)imageName.c_str(), "file", RI_RGBA, RI_NULL);
    RiFormat(16, 16, 1);
    RiTranslate(0, -1, 5);
    RiWorldBegin();
    RiAttribute(RI_PHOTON, RI_GLOBALMAP, &map, RI_NULL);
    RiSurface("matte", RI_NULL);
    RiPolygon(4, RI_P, (RtPointer)floorQuad, RI_NULL);
    CPhotonMap *second = watch("frame 2", map);
    // A map read from no file is balanced empty and reports one placeholder item
    if (second->numItems <= 1)
        fail("frame 2 read no photons from the map frame 1 wrote");
    RiWorldEnd();
    RiFrameEnd();
    RiEnd();

    checkReleased("after RiEnd");
    resetWatch();
    unlink(map);
    unlink(imageName.c_str());
}

///////////////////////////////////////////////////////////////////////
// An in-world object definition keeps the map's attributes until RiEnd
static void objectDefinition(const std::string &dir) {
    printf("object definition in the world\n");
    const std::string mapName = dir + "/objectdef.gpm";
    const char *map = mapName.c_str();
    RtInt numThreads = 1;
    RtInt emit = 100;

    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &numThreads, RI_NULL);
    RiHider("photon", RI_EMIT, &emit, RI_NULL);
    RiFormat(16, 16, 1);
    RiWorldBegin();
    RiAttributeBegin();
    RiAttribute(RI_PHOTON, RI_GLOBALMAP, &map, RI_NULL);
    RiObjectBegin();
    RiPolygon(4, RI_P, (RtPointer)floorQuad, RI_NULL); // the definition holds these attributes
    RiObjectEnd();
    RiPolygon(4, RI_P, (RtPointer)floorQuad, RI_NULL); // render() gives them the map
    watch("world", map);
    RiAttributeEnd();
    RiWorldEnd();

    // The definition still holds the attributes, and so the map
    if (deletes[0].load() != 0)
        fail("map freed at RiWorldEnd while an object definition's attributes still held it");
    else
        printf("  alive after RiWorldEnd, as the definition still holds it\n");

    RiEnd();
    checkReleased("after RiEnd");
    resetWatch();
    unlink(map);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s two-frame|objectdef\n", argv[0]);
        return 2;
    }
    char tmpl[] = "/tmp/photon_map_sessions_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    const std::string which = argv[1];
    if (which == "two-frame")
        twoFrame(dir);
    else if (which == "objectdef")
        objectDefinition(dir);
    else {
        fprintf(stderr, "unknown scenario \"%s\"\n", which.c_str());
        return 2;
    }
    rmdir(dir.c_str());
    printf("\n%s\n", failed == 0 ? "PASSED" : "FAILED");
    return failed == 0 ? 0 : 1;
}
