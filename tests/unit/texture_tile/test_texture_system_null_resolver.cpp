/**
 * tests/unit/texture_tile/test_texture_system_null_resolver.cpp
 *
 * TextureSystem_NullResolver (spec 027, T022b, FR-007, analyze finding F8).
 *
 * tests/preview/test_data_keys.cpp calls makeBrickMap(1, src, brkPath, NULL, 0, NULL, NULL) with a
 * NULL search path, so the make* functions must tolerate a nullptr resolver and treat it as "no
 * search path": the name is used as given (a path, or relative to the working directory) when the
 * file exists. This test calls makeTexture and makeBrickMap with nullptr on an existing file
 * whose name contains a directory separator and requires the result bytes to equal those made with
 * a resolver over an empty path; on a missing file it requires the same diagnostics as with the
 * resolver, and no crash.
 *
 * Same setup as that preview test: `renderMan = new CRiInterface()` around the calls (the
 * diagnostics reach the global interface).
 *
 * Red: does not compile until textureResolver.h exists (Phase 4, T047); enabled by T050a, after the
 * make* functions take the resolver and tolerate nullptr.
 */

#include "ri/texture/brickmap.h"
#include "ri/texture/pointCloud.h"
#include "ri/texture/texmake.h"
#include "ri/texture/textureResolver.h"

#include "common/algebra.h"
#include "common/diagnostics.h"
#include "common/memory.h"
#include "common/os.h"
#include "ri/parse/riInterface.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <unistd.h>

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

// A resolver over a list of directories; with an empty list it finds nothing for a bare name and,
// as the null resolver must, uses a name with a directory separator as given when it exists.
class CDirectoryResolver : public CTextureFileResolver {
    public:
        explicit CDirectoryResolver(std::vector<std::string> dirs) : dirs_(std::move(dirs)) {}

        int locate(char *result, const char *name) const override {
            if (strchr(name, '/') != nullptr && osFileExists(name)) {
                snprintf(result, OS_MAX_PATH_LENGTH, "%s", name);
                return TRUE;
            }
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

class CRecordingDiagnostics : public CDiagnosticSink {
    public:
        std::vector<std::string> messages;
        void report(EErrorCode code, EDiagnosticSeverity, const char *message) override {
            messages.push_back(std::to_string((int)code) + ":" + message);
        }
};

static std::string readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static void writePointCloudFixture(const char *path) {
    matrix from, to;
    identitym(from);
    identitym(to);
    char *names[2] = {(char *)"_radiosity", (char *)"_extra"};
    char *types[2] = {(char *)"float", (char *)"float"};
    CPointCloud *cloud = new CPointCloud(path, from, to, NULL, 2, names, types, TRUE);
    for (int i = 0; i < 8; i++) {
        float P[3] = {(float)i, 0, 0};
        float N[3] = {0, 0, 1};
        float C[2] = {0.5f, 0.25f};
        cloud->store(C, P, N, 0.1f);
    }
    delete cloud;
}

int main() {
    const char *fixtures = getenv("IMAGE_INPUT_FIXTURES_DIR");
    CHECK(fixtures != nullptr);
    const std::string source = std::string(fixtures) + "/tiny_rgb8.png"; // a name with a directory separator

    char tmpl[] = "/tmp/texture_null_resolver_XXXXXX";
    char *dir = mkdtemp(tmpl);
    CHECK(dir != nullptr);
    const std::string work = dir;

    CRiInterface *savedInterface = renderMan;
    renderMan = new CRiInterface();
    globalArena.init(); // makeTexture() reads the source into the process-wide arena (no RiBegin here)

    CDirectoryResolver emptyPath({});

    // Warm-up: the first makeTexture() of a process writes a different (smaller) file than every later one
    // (1878 against 2142 bytes for this source on the starting tree: process state, unrelated to the
    // resolver; GitHub #109), so the two outputs compared below must both be made after one throwaway bake.
    {
        const std::string warm = work + "/warm.tex";
        makeTexture(source.c_str(), warm.c_str(), &emptyPath, "clamp", "clamp", RiBoxFilter, 1, 1, 0, nullptr, nullptr);
    }

    // --- makeTexture on an existing file: the same bytes with and without a resolver
    const std::string viaNull = work + "/via_null.tex";
    const std::string viaEmpty = work + "/via_empty.tex";
    makeTexture(source.c_str(), viaNull.c_str(), nullptr, "clamp", "clamp", RiBoxFilter, 1, 1, 0, nullptr, nullptr);
    makeTexture(source.c_str(), viaEmpty.c_str(), &emptyPath, "clamp", "clamp", RiBoxFilter, 1, 1, 0, nullptr, nullptr);
    const std::string texNull = readFile(viaNull);
    const std::string texEmpty = readFile(viaEmpty);
    CHECK(!texNull.empty());
    CHECK(texNull == texEmpty);

    // --- makeBrickMap on an existing point cloud: the same bytes
    const std::string ptc = work + "/cloud.ptc";
    writePointCloudFixture(ptc.c_str());
    const char *src[1] = {ptc.c_str()};
    const std::string brkNull = work + "/via_null.brk";
    const std::string brkEmpty = work + "/via_empty.brk";
    makeBrickMap(1, src, brkNull.c_str(), nullptr, 0, nullptr, nullptr);
    makeBrickMap(1, src, brkEmpty.c_str(), &emptyPath, 0, nullptr, nullptr);
    const std::string brickNull = readFile(brkNull);
    const std::string brickEmpty = readFile(brkEmpty);
    CHECK(!brickNull.empty());
    CHECK(brickNull == brickEmpty);

    // --- a missing file: the same diagnostics, no crash, no output
    for (const char *missing : {"no_such_dir/nosuchfile_027.png", "nosuchfile_027.png"}) {
        CRecordingDiagnostics withNull, withEmpty;
        CDiagnosticSink *previous = installDiagnosticSink(&withNull);
        makeTexture(missing, (work + "/m_null.tex").c_str(), nullptr, "clamp", "clamp", RiBoxFilter, 1, 1, 0, nullptr, nullptr);
        installDiagnosticSink(&withEmpty);
        makeTexture(missing, (work + "/m_empty.tex").c_str(), &emptyPath, "clamp", "clamp", RiBoxFilter, 1, 1, 0, nullptr, nullptr);
        installDiagnosticSink(previous);
        CHECK(!withNull.messages.empty());
        CHECK(withNull.messages == withEmpty.messages);
    }

    globalArena.tini();
    delete renderMan;
    renderMan = savedInterface;

    std::string cleanup = "rm -rf '" + work + "'";
    if (system(cleanup.c_str()) != 0)
        fprintf(stderr, "could not remove %s\n", work.c_str());
    printf("TextureSystem_NullResolver: nullptr resolver == empty-path resolver (texture, brick map, missing file)\n");
    return 0;
}
