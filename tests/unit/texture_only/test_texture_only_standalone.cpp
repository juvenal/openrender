/**
 * tests/unit/texture_only/test_texture_only_standalone.cpp
 *
 * TextureOnly_Standalone (spec 027, T-T1, FR-018(a), SC-003, User Story 1 AS1).
 *
 * Texturing stands on its own: this program creates a texture system, opens a baked texture and
 * two unbaked image sources, and performs the lookups of tools/lookupprobe.sl, with no renderer
 * core, no hider, no shading library, no frame and no RiBegin. It is linked against the Texturing
 * sources compiled on a narrow include path (tests/unit/texture_only/CMakeLists.txt), so a
 * dependency on any other domain fails to compile or to link.
 *
 * The lookups are the probe's: the eight-coordinate texture() form only (it carries its own
 * footprint), "samples" 1, a box filter, no blur. Each result is printed the way the probe's
 * printf prints it ("<tag> <footprint> (%f,%f,%f)") and compared, as a set of lines, with
 * specs/027-texture-system/tools/master-lookup-values.txt, which was produced by the starting
 * tree's interpreter (T014). environment() and shadow() are not here: they take one direction /
 * one point and Shading derives their footprint (covered by TextureSystem_StreamCursor, the
 * substitute test and the byte-identical renders).
 *
 * Red until Phase 5 (textureSystem.h did not exist) and until Phases 4-8 took the renderer-core
 * headers out of the Texturing sources; enabled by T092 (Phase 8). The input is
 *   argv[1] the expected-values file
 *   argv[2] tests/unit/texture_tile/fixtures      (concurrency_rgb.tex)
 *   argv[3] tests/unit/image_input/fixtures       (tiny_rgb8.png)
 * and the non-power-of-two PNG nonpot_rgb.png is read from argv[2] as well.
 */

#include "ri/texture/textureResolver.h"
#include "ri/texture/textureSystem.h"

#include "common/memory.h"
#include "common/os.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                              \
        }                                                                          \
    } while (0)

// A resolver over a list of directories: the test's only way to find a file, standing in for the
// renderer core's search-path resolver.
class CDirectoryResolver : public CTextureFileResolver {
    public:
        explicit CDirectoryResolver(std::vector<std::string> dirs) : dirs_(std::move(dirs)) {}

        int locate(char *result, const char *name) const override {
            for (const std::string &dir : dirs_) {
                const std::string path = dir + "/" + name;
                if (osFileExists(path.c_str())) {
                    snprintf(result, OS_MAX_PATH_LENGTH, "%s", path.c_str());
                    return TRUE;
                }
            }
            return FALSE;
        }

    private:
        std::vector<std::string> dirs_;
};

struct SFootprint {
        char tag;
        float s[4];
        float t[4];
};

// The corners of tools/lookupprobe.sl, in the order the interpreter's eight-coordinate form hands
// them to lookup4: cs = {s0, s1, s2, s3}, ct = {t0, t1, t2, t3}.
static const SFootprint kFootprints[] = {
    {'A', {0.30f, 0.31f, 0.30f, 0.31f}, {0.40f, 0.40f, 0.41f, 0.41f}},
    {'B', {0.25f, 0.35f, 0.25f, 0.35f}, {0.25f, 0.25f, 0.35f, 0.35f}},
    {'C', {0.10f, 0.60f, 0.10f, 0.60f}, {0.10f, 0.10f, 0.60f, 0.60f}},
    {'D', {0.95f, 1.05f, 0.95f, 1.05f}, {0.95f, 0.95f, 1.05f, 1.05f}},
    {'E', {0.20f, 0.45f, 0.22f, 0.47f}, {0.30f, 0.32f, 0.34f, 0.36f}},
};

struct SFixture {
        const char *tag;
        const char *name;
};

static const SFixture kFixtures[] = {
    {"baked", "concurrency_rgb.tex"},
    {"unbaked", "tiny_rgb8.png"},
    {"nonpot", "nonpot_rgb.png"},
};

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <expected-values> <texture_tile fixtures> <image_input fixtures>\n", argv[0]);
        return 2;
    }

    std::vector<std::string> expected;
    {
        std::ifstream in(argv[1]);
        CHECK(in.good());
        std::string line;
        while (std::getline(in, line))
            if (!line.empty())
                expected.push_back(line);
        std::sort(expected.begin(), expected.end());
        CHECK(!expected.empty());
    }

    // Synthesizing an unbaked source decodes into the process-wide arena, which RiBegin initializes in a
    // renderer and nothing else does (libRIBCommon owns the arena; there is no frame here)
    globalArena.init();

    CDirectoryResolver resolver({argv[2], argv[3]});

    CTextureSystemSettings settings = {};
    settings.numThreads = 1;
    settings.maxMemory = 64 * 1024 * 1024;
    settings.resolver = &resolver;
    settings.diskCacheEnabled = false;
    settings.diskCacheDirectory = nullptr;
    static const matrix kIdentity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    memcpy(settings.toWorld, kIdentity, sizeof(matrix));
    settings.loadMutex = nullptr;

    CTextureSystem *system = createTextureSystem(settings);
    CHECK(system != nullptr);
    TexturePerthread *perthread = system->perthread(0);
    CHECK(perthread != nullptr);

    std::vector<std::string> actual;
    for (const SFixture &fixture : kFixtures) {
        TextureHandle *handle = system->handle(fixture.name, ETextureKind::Texture);
        CHECK(handle != nullptr);

        for (const SFootprint &fp : kFootprints) {
            TextureOpt opt = {};
            opt.filter = RiBoxFilter;
            opt.blur = 0;
            opt.width = opt.swidth = opt.twidth = 1;
            opt.fill = 0;
            opt.samples = 1;
            opt.traceSamples = 1;
            opt.bias = 0;
            opt.stream = {openrender::sampling::Backend::Sobol, {0, 0}, 0};

            float result[3] = {0, 0, 0};
            system->texture(handle, opt, perthread, fp.s, fp.t, result);

            char line[128];
            snprintf(line, sizeof(line), "%s %c (%f,%f,%f)", fixture.tag, fp.tag, result[0], result[1], result[2]);
            actual.push_back(line);
        }
    }
    std::sort(actual.begin(), actual.end());

    int failures = 0;
    if (actual.size() != expected.size()) {
        fprintf(stderr, "FAIL: %zu lookups computed, %zu expected\n", actual.size(), expected.size());
        failures++;
    }
    for (size_t i = 0; i < actual.size() && i < expected.size(); i++) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "FAIL: computed \"%s\", expected \"%s\"\n", actual[i].c_str(), expected[i].c_str());
            failures++;
        }
    }

    delete system;
    globalArena.tini();
    if (failures == 0)
        printf("TextureOnly_Standalone: %zu lookups equal the starting tree's values\n", actual.size());
    return failures == 0 ? 0 : 1;
}
