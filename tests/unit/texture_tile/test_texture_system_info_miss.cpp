/**
 * tests/unit/texture_tile/test_texture_system_info_miss.cpp
 *
 * TextureSystem_InfoMiss (spec 027, T022b, FR-002, plan Q2, User Story 2 AS3).
 *
 * textureinfo() on a missing file returns "not found" and must not poison the cache: today's
 * getTextureInfo returns nullptr and inserts nothing, so every later request probes again, and a
 * later texture() of the same name still gets the substitute and its one diagnostic. The new
 * system keeps exactly that:
 *   - handle("nosuchfile.tex", ETextureKind::Info) returns nullptr;
 *   - nothing is cached: a second call with the counting resolver locates again (locate count 2);
 *   - there are zero diagnostics;
 *   - a following ETextureKind::Texture request for the same name still yields a substitute
 *     (non-null) with exactly one CODE_NOFILE.
 * An Info request for an existing file returns a handle usable with describe().
 *
 * Red: does not compile until textureSystem.h exists (Phase 5, T058); enabled by T063a.
 */

#include "texture_system_test_doubles.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

int main() {
    const char *dir = getenv("TEXTURE_TILE_FIXTURES_DIR");
    CDirectoryResolver directories({dir ? dir : "tests/unit/texture_tile/fixtures"});
    CCountingResolver counting(&directories);

    CCountingDiagnostics diagnostics;
    CDiagnosticSink *previous = installDiagnosticSink(&diagnostics);

    CTextureSystem *system = createTextureSystem(testSystemSettings(&counting, 1));
    CHECK(system != nullptr);

    const char *missing = "nosuchfile_027_info.tex";

    // A miss: nullptr, nothing cached (the second request locates again), no diagnostic
    CHECK(system->handle(missing, ETextureKind::Info) == nullptr);
    CHECK(counting.count(missing) == 1);
    CHECK(system->handle(missing, ETextureKind::Info) == nullptr);
    CHECK(counting.count(missing) == 2);
    CHECK(diagnostics.total() == 0);

    // The same name as a texture is still a substitute with exactly one CODE_NOFILE
    TextureHandle *substitute = system->handle(missing, ETextureKind::Texture);
    CHECK(substitute != nullptr);
    CHECK(diagnostics.noFile() == 1);
    CHECK(system->handle(missing, ETextureKind::Texture) == substitute); // cached now
    CHECK(diagnostics.noFile() == 1);

    // A hit: a handle that describes itself
    TextureHandle *found = system->handle("concurrency_rgb.tex", ETextureKind::Info);
    CHECK(found != nullptr);
    TextureInfo info = {};
    system->describe(found, info);
    CHECK(info.resolution[0] == 512 && info.resolution[1] == 512);
    CHECK(info.numChannels == 3);

    delete system;
    installDiagnosticSink(previous);
    printf("TextureSystem_InfoMiss: a miss returns nullptr, caches nothing and reports nothing\n");
    return 0;
}
