/**
 * tests/unit/image_input/test_image_input_errors.cpp
 *
 * Cross-format negative-case unit tests for spec 018 (T031, FR-008/SC-004):
 * createImageInput() must return nullptr for an unsupported extension, and
 * each decoder's open() must return false (not crash) for a file with a
 * supported extension but corrupted/invalid content for that format.
 */

#include "imageInput.h"
#include "imageInputPng.h"
#include "imageInputTiff.h"
#ifdef HAVE_OPENEXR
#include "imageInputExr.h"
#endif
#include "imageInputRgbe.h"
#include "ri.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h> // mkdtemp() -- Linux/macOS only, per constitution VI

// assert() compiles away under -DNDEBUG (Release builds); use an
// always-active check instead (same pattern as the other decoder tests).
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

// A real OS temp directory (never the source tree, regardless of the
// test's working directory) -- created once per process, removed on exit.
static std::string scratchDir() {
    static std::string dir;
    if (dir.empty()) {
        std::string tmpl = std::string(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp") + "/image_input_errors_XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        char *made = mkdtemp(buf.data());
        CHECK(made != NULL);
        dir = made;
    }
    return dir;
}

static std::string writeGarbage(const char *name, const char *content) {
    std::string path = scratchDir() + "/" + name;
    FILE *fp = fopen(path.c_str(), "wb");
    CHECK(fp != NULL);
    fwrite(content, 1, strlen(content), fp);
    fclose(fp);
    return path;
}

int main() {
    RiBegin(RI_NULL);

    // createImageInput() must return nullptr for an unrecognized extension --
    // no TIFF fallback, no crash. Per data-model.md's Format Registration
    // Table and contracts/image-input-interface.md rule 5.
    {
        std::string garbage = writeGarbage("fake.bmp", "not a real image");
        CImageInput *in = createImageInput(garbage.c_str());
        CHECK(in == NULL);
    }
    // Also a file with no extension at all.
    {
        std::string garbage = writeGarbage("no_extension_at_all", "not a real image");
        CImageInput *in = createImageInput(garbage.c_str());
        CHECK(in == NULL);
    }

    // Each decoder must fail cleanly (open() returns false), not crash, on
    // a file with the right extension but corrupted/invalid content.
    {
        std::string garbage = writeGarbage("corrupt.tif", "this is not a tiff file");
        CTiffImageInput input;
        CImageInfo info;
        CHECK(!input.open(garbage.c_str(), info));
    }
    {
        std::string garbage = writeGarbage("corrupt.png", "this is not a png file");
        CPngImageInput input;
        CImageInfo info;
        CHECK(!input.open(garbage.c_str(), info));
    }
#ifdef HAVE_OPENEXR
    {
        std::string garbage = writeGarbage("corrupt.exr", "this is not an exr file");
        COpenExrImageInput input;
        CImageInfo info;
        CHECK(!input.open(garbage.c_str(), info));
    }
#endif
    {
        std::string garbage = writeGarbage("corrupt.hdr", "this is not an rgbe file\n\n");
        CRgbeImageInput input;
        CImageInfo info;
        CHECK(!input.open(garbage.c_str(), info));
    }

    // And via the factory end-to-end, for good measure: a mismatched
    // extension (claims .png, is actually garbage) must still fail cleanly
    // through the full createImageInput()->open() path, not just the
    // concrete class in isolation.
    {
        std::string garbage = writeGarbage("mismatched.png", "garbage content");
        CImageInput *in = createImageInput(garbage.c_str());
        CHECK(in != NULL); // .png suffix matches, so a CPngImageInput is created
        CImageInfo info;
        CHECK(!in->open(garbage.c_str(), info));
        delete in;
    }

    RiEnd();

    printf("test_image_input_errors: ALL PASS\n");
    return 0;
}
