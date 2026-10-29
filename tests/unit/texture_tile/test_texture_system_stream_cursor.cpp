/**
 * tests/unit/texture_tile/test_texture_system_stream_cursor.cpp
 *
 * TextureSystem_StreamCursor (spec 027, T-K1, T023, FR-005, FR-018(d), User Story 2 AS4).
 *
 * The sample stream (spec 039) is a counter: every stochastic draw of a lookup takes the next
 * dimension of the caller's TextureSampleStream, and the caller reads the advanced cursor back, so
 * the dimensions a lookup consumes decide every later draw of the shading point. This test
 * pins, per lookup class, how many dimensions one lookup consumes (data-model.md section 5, derived
 * from the starting tree's code), and that the advanced cursor is visible to the caller.
 *
 * No renderer, no frame, no RiBegin: a texture system over a directory resolver and a TextureOpt
 * with a known stream starting at a non-zero dimension. Fixtures:
 *   baked 2D            tests/unit/texture_tile/fixtures/concurrency_rgb.tex
 *   unbaked 2D          tests/unit/image_input/fixtures/tiny_rgb8.png (synthesized into a pyramid)
 *   regular 2D          tests/unit/image_input/fixtures/tiny_rgb.tif (a plain TIFF)
 *   shadow              examples/rib/tests/shadow-source.tex (a baked shadow map)
 *   cubic environment   six sides baked here with makeCubicEnvironment from medium_rgb.tif
 *   spherical, cylindrical  baked here with makeSphericalEnvironment / makeCylindericalEnvironment
 *   deep shadow         generated at test time: orender renders deep-shadow-fixture.rib through
 *                       the "tsm" display driver into the working directory
 * Expected dimensions, with S = opt.samples and T = opt.traceSamples:
 *   baked 2D                 2 S
 *   unbaked 2D (PNG)         2 S, blur or not
 *   regular 2D (plain TIFF)  2 S, and 4 S when blur > 0
 *   shadow, deep shadow      4 T
 *   cubic environment        2 T
 *   spherical, cylindrical   2 S (the side texture draws from the same stream)
 *   texturePoint, substitutes  0
 *
 * Red: does not compile until textureSystem.h exists (T033); enabled in Phase 6 (T081).
 * Environment: ORENDER_BIN, TEXTURES-like fixture directories are passed as arguments.
 *   argv[1] tests/unit/texture_tile/fixtures
 *   argv[2] tests/unit/image_input/fixtures
 *   argv[3] examples/rib/tests
 *   argv[4] tests/unit/texture_tile/deep-shadow-fixture.rib
 * and the environment of an orender run (SHADERS, ORENDERHOME, DISPLAYS) plus ORENDER_BIN.
 */

#include "texture_system_test_doubles.h"

#include "ri/texture/texmake.h"

#include "common/diagnostics.h"
#include "common/memory.h"
#include "ri/parse/riInterface.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <unistd.h>

static int g_failures = 0;
#define EXPECT_EQ_DIMS(what, got, want)                                                                     \
    do {                                                                                                    \
        if ((got) != (want)) {                                                                              \
            fprintf(stderr, "FAIL %s: consumed %u dimensions, expected %u (%s:%d)\n", what, (unsigned)(got), \
                    (unsigned)(want), __FILE__, __LINE__);                                                  \
            g_failures++;                                                                                   \
        }                                                                                                   \
        else {                                                                                              \
            printf("ok   %s: %u dimensions\n", what, (unsigned)(got));                                      \
        }                                                                                                   \
    } while (0)
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

static const uint32_t kStartDim = 5; // a non-zero start: the cursor is relative, and visible to the caller

static TextureOpt makeOpt(float samples, float traceSamples, float blur) {
    TextureOpt opt = {};
    opt.filter = RiBoxFilter;
    opt.blur = blur;
    opt.width = opt.swidth = opt.twidth = 1;
    opt.fill = 0;
    opt.samples = samples;
    opt.traceSamples = traceSamples;
    opt.bias = 0.01f;
    opt.stream = {openrender::sampling::Backend::Sobol, {0x1234abcdULL, 7}, kStartDim};
    return opt;
}

static const float kS4[4] = {0.30f, 0.34f, 0.30f, 0.34f};
static const float kT4[4] = {0.40f, 0.40f, 0.44f, 0.44f};
// A direction and three neighbours (environment), a point and three neighbours (shadow)
static const float kD0[3] = {0.30f, 0.40f, 0.80f}, kD1[3] = {0.32f, 0.40f, 0.80f}, kD2[3] = {0.30f, 0.42f, 0.80f},
                   kD3[3] = {0.32f, 0.42f, 0.80f};

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <texture_tile fixtures> <image_input fixtures> <examples/rib/tests> <deep-shadow-fixture.rib>\n", argv[0]);
        return 2;
    }
    const char *orender = getenv("ORENDER_BIN");
    CHECK(orender != nullptr);

    char tmpl[] = "/tmp/texture_stream_cursor_XXXXXX";
    char *workDir = mkdtemp(tmpl);
    CHECK(workDir != nullptr);
    const std::string work = workDir;

    // The make* functions report through the global interface, as in tests/preview/test_data_keys.cpp
    CRiInterface *savedInterface = renderMan;
    renderMan = new CRiInterface();
    globalArena.init(); // the make* functions read their sources into the process-wide arena (no RiBegin here)

    // --- fixtures that are built here
    CDirectoryResolver sources({argv[3], argv[1], argv[2]});
    const std::string envSource = "medium_rgb.tif"; // a plain source: env-source.tex is itself baked, which the make* functions refuse
    const std::string cubic = work + "/cubic.env";
    const std::string spherical = work + "/spherical.env";
    const std::string cylindrical = work + "/cylindrical.env";
    makeCubicEnvironment(envSource.c_str(), envSource.c_str(), envSource.c_str(), envSource.c_str(), envSource.c_str(),
                         envSource.c_str(), cubic.c_str(), "clamp", "clamp", &sources, RiBoxFilter, 1, 1, 0, nullptr, nullptr, TRUE);
    makeSphericalEnvironment(envSource.c_str(), spherical.c_str(), &sources, "periodic", "clamp", RiBoxFilter, 1, 1, 0, nullptr, nullptr);
    makeCylindericalEnvironment(envSource.c_str(), cylindrical.c_str(), &sources, "periodic", "clamp", RiBoxFilter, 1, 1, 0, nullptr, nullptr);

    // The deep shadow map: an orender run into the working directory
    {
        const std::string command = "cd '" + work + "' && '" + orender + "' -t:1 '" + argv[4] + "' > orender.log 2>&1";
        CHECK(system(command.c_str()) == 0);
    }
    const std::string deep = work + "/deep-shadow-fixture.tsm";
    CHECK(osFileExists(deep.c_str()));

    // --- the system
    CDirectoryResolver search({argv[1], argv[2], argv[3], work});
    CTextureSystem *textures = createTextureSystem(testSystemSettings(&search, 1));
    CHECK(textures != nullptr);
    TexturePerthread *pt = textures->perthread(0);
    CHECK(pt != nullptr);
    float result[3];

    // --- 2D lookups: samples = 3
    {
        TextureHandle *baked = textures->handle("concurrency_rgb.tex", ETextureKind::Texture);
        TextureOpt opt = makeOpt(3, 1, 0);
        textures->texture(baked, opt, pt, kS4, kT4, result);
        EXPECT_EQ_DIMS("baked 2D lookup4", opt.stream.dim - kStartDim, 2u * 3u);

        TextureHandle *unbaked = textures->handle("tiny_rgb8.png", ETextureKind::Texture);
        opt = makeOpt(3, 1, 0);
        textures->texture(unbaked, opt, pt, kS4, kT4, result);
        EXPECT_EQ_DIMS("unbaked 2D lookup4", opt.stream.dim - kStartDim, 2u * 3u);

        // A PNG is synthesized into a pyramid like a baked texture: blur only picks the level, it draws nothing
        opt = makeOpt(3, 1, 0.02f);
        textures->texture(unbaked, opt, pt, kS4, kT4, result);
        EXPECT_EQ_DIMS("unbaked 2D (PNG) lookup4 with blur", opt.stream.dim - kStartDim, 2u * 3u);

        // A plain TIFF is a regular texture (CRegularTexture), the one class whose blur draws a second pair per sample
        TextureHandle *regular = textures->handle("tiny_rgb.tif", ETextureKind::Texture);
        opt = makeOpt(3, 1, 0);
        textures->texture(regular, opt, pt, kS4, kT4, result);
        EXPECT_EQ_DIMS("regular 2D lookup4", opt.stream.dim - kStartDim, 2u * 3u);

        opt = makeOpt(3, 1, 0.02f);
        textures->texture(regular, opt, pt, kS4, kT4, result);
        EXPECT_EQ_DIMS("regular 2D lookup4 with blur", opt.stream.dim - kStartDim, 4u * 3u);

        opt = makeOpt(3, 1, 0);
        textures->texturePoint(baked, opt, pt, 0.3f, 0.4f, result);
        EXPECT_EQ_DIMS("baked 2D point lookup", opt.stream.dim - kStartDim, 0u);
        textures->texturePoint(unbaked, opt, pt, 0.3f, 0.4f, result);
        EXPECT_EQ_DIMS("unbaked 2D point lookup", opt.stream.dim - kStartDim, 0u);
    }

    // --- environment, shadow and deep shadow: traceSamples = 4, samples = 3
    {
        TextureOpt opt = makeOpt(3, 4, 0);
        TextureHandle *shadow = textures->handle("shadow-source.tex", ETextureKind::Environment);
        textures->environment(shadow, opt, pt, kD0, kD1, kD2, kD3, result);
        EXPECT_EQ_DIMS("shadow (CShadow)", opt.stream.dim - kStartDim, 4u * 4u);

        opt = makeOpt(3, 4, 0);
        TextureHandle *deepMap = textures->handle("deep-shadow-fixture.tsm", ETextureKind::Environment);
        textures->environment(deepMap, opt, pt, kD0, kD1, kD2, kD3, result);
        EXPECT_EQ_DIMS("deep shadow (CDeepShadow)", opt.stream.dim - kStartDim, 4u * 4u);

        opt = makeOpt(3, 4, 0);
        TextureHandle *cube = textures->handle("cubic.env", ETextureKind::Environment);
        textures->environment(cube, opt, pt, kD0, kD1, kD2, kD3, result);
        EXPECT_EQ_DIMS("cubic environment", opt.stream.dim - kStartDim, 2u * 4u);

        // Spherical and cylindrical: the side texture's lookup4, drawn from the same stream
        opt = makeOpt(3, 4, 0);
        TextureHandle *sph = textures->handle("spherical.env", ETextureKind::Environment);
        textures->environment(sph, opt, pt, kD0, kD1, kD2, kD3, result);
        EXPECT_EQ_DIMS("spherical environment", opt.stream.dim - kStartDim, 2u * 3u);

        opt = makeOpt(3, 4, 0);
        TextureHandle *cyl = textures->handle("cylindrical.env", ETextureKind::Environment);
        textures->environment(cyl, opt, pt, kD0, kD1, kD2, kD3, result);
        EXPECT_EQ_DIMS("cylindrical environment", opt.stream.dim - kStartDim, 2u * 3u);
    }

    // --- substitutes (a missing file): no draws, and the fill value
    {
        CCountingDiagnostics quiet;
        CDiagnosticSink *previous = installDiagnosticSink(&quiet);
        TextureOpt opt = makeOpt(3, 4, 0);
        opt.fill = 0.25f;
        TextureHandle *missingTexture = textures->handle("nosuchfile_027_a.tex", ETextureKind::Texture);
        textures->texture(missingTexture, opt, pt, kS4, kT4, result);
        EXPECT_EQ_DIMS("substitute texture", opt.stream.dim - kStartDim, 0u);
        if (result[0] != 0.25f || result[1] != 0.25f || result[2] != 0.25f) {
            fprintf(stderr, "FAIL substitute texture returned (%f %f %f), expected the fill 0.25\n", result[0], result[1], result[2]);
            g_failures++;
        }

        opt = makeOpt(3, 4, 0);
        opt.fill = 0.25f;
        TextureHandle *missingEnv = textures->handle("nosuchfile_027_b.env", ETextureKind::Environment);
        textures->environment(missingEnv, opt, pt, kD0, kD1, kD2, kD3, result);
        EXPECT_EQ_DIMS("substitute environment", opt.stream.dim - kStartDim, 0u);
        installDiagnosticSink(previous);
    }

    delete textures;
    globalArena.tini();
    delete renderMan;
    renderMan = savedInterface;
    std::string cleanup = "rm -rf '" + work + "'";
    if (system(cleanup.c_str()) != 0)
        fprintf(stderr, "could not remove %s\n", work.c_str());

    if (g_failures)
        fprintf(stderr, "%d failure(s)\n", g_failures);
    return g_failures ? 1 : 0;
}
