/**
 * tests/shading_parity/test_named_space_casts_jit.cpp
 *
 * Regression test for GitHub #123 and #124: a point, vector or normal written
 * in a named space ("world" here) must reach the current (camera) space the
 * same way under the interpreter (.rslo) and the LLVM JIT (.slo).
 *
 *   #123: a parameter default is converted once, at bind time, by the shader's
 *         init section. Under the JIT there is no shading context then, and the
 *         bind-time fallback resolved only "shader": point "world" (1, 2, 3)
 *         stayed (1, 2, 3), unconverted.
 *   #124: the JIT lowered the vfrom instruction (vector "space" (...)) to
 *         op_vtransform, which goes current -> named instead of named ->
 *         current, so the vector came out with the inverse rotation. This one
 *         also hits a cast in the shader body, evaluated per shading point.
 *
 * Three cases, each rendered on both backends, each asserted against the value
 * derived by hand from the camera transform (Translate 0 0 5, Rotate 90 0 0 1:
 * world (x, y, z) -> camera (-y, x, z + 5) for a point, (-y, x, z) for a vector):
 *
 *   light   point  p = point "world" (1, 2, 3)   -> (-2, 1, 8)   (#123)
 *   light   vector p = vector "world" (1, 2, 3)  -> (-2, 1, 3)   (#123 and #124)
 *   surface body:  vector "world" (1, 2, 3)      -> (-2, 1, 3)   (#124)
 *
 * The value is written to Cl (light) or Ci (surface) and read back from the first
 * pixel of a 2x2 uncompressed float TIFF. A light case's receiving surface always
 * runs on the interpreter (Attribute "shade" "shaderformat" ["rslo"]), so only the
 * light's backend varies.
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

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

static bool writeFile(const std::string &path, const std::string &content) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

static std::vector<unsigned char> readBytes(const std::string &path) {
    std::vector<unsigned char> d;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return d;
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        d.insert(d.end(), buf, buf + n);
    fclose(f);
    return d;
}

static bool runOshader(const char *oshaderBin, const std::string &srcPath, const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                      "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// orender's exit code is not a useful signal here (see test_printf_jit.cpp): the
// image file is.
static void runOrender(const char *orenderBin, const char *displaysDir, const std::string &shadersDir,
                       const std::string &ribPath, const std::string &logPath) {
    std::string cmd = std::string("SHADERS=\"") + shadersDir + "\" ORENDERHOME=\"" + shadersDir +
                      "\" DISPLAYS=\"" + displaysDir + "\" \"" + orenderBin + "\" \"" + ribPath + "\" > \"" +
                      logPath + "\" 2>&1";
    int rc = system(cmd.c_str());
    (void)rc;
}

// The first pixel of an uncompressed, 32-bit float, chunky RGB TIFF (what the
// "file" display writes for Quantize 0 and "compression" "none").
static bool readFirstPixel(const std::string &path, float rgb[3]) {
    const std::vector<unsigned char> d = readBytes(path);
    if (d.size() < 8)
        return false;
    const bool le = d[0] == 'I';
    auto u16 = [&](size_t o) -> uint32_t {
        if (o + 2 > d.size())
            return 0;
        return le ? (uint32_t)(d[o] | (d[o + 1] << 8)) : (uint32_t)((d[o] << 8) | d[o + 1]);
    };
    auto u32 = [&](size_t o) -> uint32_t {
        if (o + 4 > d.size())
            return 0;
        return le ? (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8) | ((uint32_t)d[o + 2] << 16) | ((uint32_t)d[o + 3] << 24)
                  : ((uint32_t)d[o] << 24) | ((uint32_t)d[o + 1] << 16) | ((uint32_t)d[o + 2] << 8) | (uint32_t)d[o + 3];
    };
    const uint32_t ifd = u32(4);
    const uint32_t count = u16(ifd);
    uint32_t compression = 1, bits = 0, format = 1, strip = 0;
    bool haveStrip = false;
    for (uint32_t i = 0; i < count; i++) {
        const size_t e = ifd + 2 + 12 * i;
        const uint32_t tag = u16(e), type = u16(e + 2), n = u32(e + 4);
        const uint32_t value = (type == 3 && n == 1) ? u16(e + 8) : u32(e + 8);
        if (tag == 259)
            compression = value;
        else if (tag == 258)
            bits = (n == 1) ? value : u16(value);
        else if (tag == 339)
            format = (n == 1) ? value : u16(value);
        else if (tag == 273) {
            strip = (n == 1) ? value : (type == 3 ? u16(value) : u32(value));
            haveStrip = true;
        }
    }
    if (compression != 1 || bits != 32 || format != 3 || !haveStrip || strip + 12 > d.size())
        return false;
    for (int c = 0; c < 3; c++) {
        const uint32_t w = u32(strip + 4 * c);
        memcpy(&rgb[c], &w, sizeof(float));
    }
    return true;
}

struct Case {
    const char *name;
    bool light;          // a light (value in Cl) or a surface (value in Ci)
    const char *source;  // the shader, named after the case
    float expected[3];   // camera space
};

int main() {
    printf("GitHub #123/#124: named-space casts and parameter defaults, .rslo vs .slo\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_named_space_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    // The receiving surface of the light cases: the sum of Cl over every light.
    const std::string probe = "named_space_probe";
    const std::string probeSrc = "surface " + probe +
                                 "() {\n"
                                 "    color c = 0;\n"
                                 "    illuminance(P) { c += Cl; }\n"
                                 "    Ci = c; Oi = 1;\n"
                                 "}\n";
    EXPECT_TRUE(writeFile(dir + "/" + probe + ".sl", probeSrc));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + probe + ".sl", dir + "/" + probe + ".rslo", false));

    const Case cases[] = {
        {"named_space_light_point_world", true,
         "light named_space_light_point_world(point p = point \"world\" (1, 2, 3)) {\n"
         "    illuminate(point (0, 0, -1000)) { Cl = color(xcomp(p), ycomp(p), zcomp(p)); }\n"
         "}\n",
         {-2, 1, 8}},
        {"named_space_light_vector_world", true,
         "light named_space_light_vector_world(vector p = vector \"world\" (1, 2, 3)) {\n"
         "    illuminate(point (0, 0, -1000)) { Cl = color(xcomp(p), ycomp(p), zcomp(p)); }\n"
         "}\n",
         {-2, 1, 3}},
        {"named_space_body_vector_world", false,
         "surface named_space_body_vector_world() {\n"
         "    vector p = vector \"world\" (1, 2, 3);\n"
         "    Ci = color(xcomp(p), ycomp(p), zcomp(p)); Oi = 1;\n"
         "}\n",
         {-2, 1, 3}},
    };

    for (const Case &c : cases) {
        const std::string src = dir + "/" + c.name + ".sl";
        EXPECT_TRUE(writeFile(src, c.source));
        EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + c.name + ".rslo", false));
        EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + c.name + ".slo", true));

        for (const char *format : {"rslo", "slo"}) {
            const std::string base = dir + "/" + c.name + "_" + format;
            std::string body;
            if (c.light)
                body = std::string("LightSource \"") + c.name + "\" 1\n" +
                       "Attribute \"shade\" \"shaderformat\" [\"rslo\"]\n" + "Surface \"" + probe + "\"\n";
            else
                body = std::string("Surface \"") + c.name + "\"\n";
            writeFile(base + ".rib",
                      std::string("Option \"shaderformat\" \"default\" [\"") + format + "\"]\n" +
                          "Hider \"raytrace\" \"jitter\" [0]\n"
                          "Option \"sampling\" \"sampler\" [\"sobol\"]\n"
                          "Option \"sampling\" \"seed\" [1]\n"
                          "Option \"limits\" \"numthreads\" [1]\n"
                          "PixelSamples 1 1\n"
                          "PixelFilter \"box\" 1 1\n"
                          "Format 2 2 1\n"
                          "Quantize \"rgba\" 0 0 0 0\n"
                          "Display \"" +
                          base + ".tif\" \"file\" \"rgb\" \"string compression\" [\"none\"]\n" +
                          "Projection \"orthographic\"\n"
                          "Translate 0 0 5\n"
                          "Rotate 90 0 0 1\n"
                          "WorldBegin\n" +
                          body +
                          "Polygon \"P\" [-50 -50 0  50 -50 0  50 50 0  -50 50 0]\n"
                          "WorldEnd\n");
            runOrender(orenderBin, displaysDir, dir, base + ".rib", base + ".log");

            float rgb[3] = {NAN, NAN, NAN};
            const bool read = readFirstPixel(base + ".tif", rgb);
            const bool match = read && fabsf(rgb[0] - c.expected[0]) < 1e-4f &&
                               fabsf(rgb[1] - c.expected[1]) < 1e-4f && fabsf(rgb[2] - c.expected[2]) < 1e-4f;
            printf("  %-32s .%-4s got (%g, %g, %g), expected (%g, %g, %g): %s\n", c.name, format, rgb[0], rgb[1],
                   rgb[2], c.expected[0], c.expected[1], c.expected[2], match ? "ok" : "WRONG");
            EXPECT_TRUE(read);
            EXPECT_TRUE(match);
        }
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
