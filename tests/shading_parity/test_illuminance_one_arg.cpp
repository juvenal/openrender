/**
 * tests/shading_parity/test_illuminance_one_arg.cpp
 *
 * Regression test for GitHub #126: a .slo surface whose only lighting call is the
 * one-argument illuminance(P) gathered no light. The compiler writes that form with
 * no N and angle operands; the JIT emitter read them anyway, found nothing and
 * skipped the whole loop body without a diagnostic. The interpreter runs the form
 * with N = (0,0,1) and a cone of PI, that is, every light.
 *
 * The matrix is surface (.rslo, .slo) x light (.rslo, .slo) for illuminance(P),
 * with the three-argument illuminance(P, N, PI) as a control. Every image must equal
 * the .rslo/.rslo one, and that one must show the light (the constant (1,2,3)).
 *
 * Every render runs in its own process group under a time limit.
 */

#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
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

static const int kSize = 8;       // image is kSize x kSize
static const int kTimeoutSec = 8; // per render; a healthy one takes well under a second

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

enum RenderEnd { RENDER_DONE,
                 RENDER_CRASHED,
                 RENDER_TIMEOUT,
                 RENDER_NOSTART };

// Runs orender on one thread in its own process group, with a time limit. orender's exit status is not a useful
// signal (it is non-zero for the unrelated "Failed to find shader defaultsurface"), so DONE means "exited by itself".
static RenderEnd runOrender(const char *orenderBin, const char *displaysDir, const std::string &shadersDir, const std::string &ribPath, const std::string &logPath, int *signalOut) {
    *signalOut = 0;
    const pid_t pid = fork();
    if (pid < 0)
        return RENDER_NOSTART;
    if (pid == 0) {
        setpgid(0, 0);
        const int fd = open(logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
        }
        setenv("SHADERS", shadersDir.c_str(), 1);
        setenv("ORENDERHOME", shadersDir.c_str(), 1);
        setenv("DISPLAYS", displaysDir, 1);
        execl(orenderBin, orenderBin, "-t:1", ribPath.c_str(), (char *)nullptr);
        _exit(127);
    }
    setpgid(pid, pid);
    int status = 0;
    for (int waited = 0; waited < kTimeoutSec * 20; ++waited) {
        const pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            if (WIFSIGNALED(status)) {
                *signalOut = WTERMSIG(status);
                return RENDER_CRASHED;
            }
            return RENDER_DONE;
        }
        usleep(50 * 1000);
    }
    kill(-pid, SIGKILL);
    waitpid(pid, &status, 0);
    return RENDER_TIMEOUT;
}

// The pixels of an uncompressed, 32-bit float, chunky RGB TIFF in one strip (what the "file" display writes for
// Quantize 0 and "compression" "none").
static bool readImage(const std::string &path, std::vector<float> &rgb) {
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
    const size_t bytes = (size_t)kSize * kSize * 3 * sizeof(float);
    if (compression != 1 || bits != 32 || format != 3 || !haveStrip || strip + bytes > d.size())
        return false;
    rgb.resize((size_t)kSize * kSize * 3);
    for (size_t i = 0; i < rgb.size(); i++) {
        const uint32_t w = u32(strip + 4 * i);
        memcpy(&rgb[i], &w, sizeof(float));
    }
    return true;
}

static const char *kLightSource =
    "light constl(float intensity = 1; point from = point \"shader\" (0,0,-5);) {\n"
    "    illuminate(from) { Cl = intensity * color(1, 2, 3); }\n"
    "}\n";

struct SurfaceCase {
        const char *name;
        const char *source;
};

static const SurfaceCase surfaces[] = {
    {"illum1",
     "surface illum1() {\n"
     "    color c = 0;\n"
     "    illuminance(P) { c += Cl; }\n"
     "    Ci = c;\n"
     "}\n"},
    {"illum3",
     "surface illum3() {\n"
     "    color c = 0;\n"
     "    illuminance(P, normalize(N), PI) { c += Cl; }\n"
     "    Ci = c;\n"
     "}\n"},
};

static std::string sceneRib(const std::string &tif, const char *lightFormat, const char *surfaceFormat, const char *surface) {
    return std::string("Format ") + std::to_string(kSize) + " " + std::to_string(kSize) + " 1\n" +
           "PixelSamples 1 1\n"
           "PixelFilter \"box\" 1 1\n"
           "Quantize \"rgba\" 0 0 0 0\n"
           "Display \"" +
           tif + "\" \"file\" \"rgb\" \"string compression\" [\"none\"]\n" +
           "Projection \"perspective\" \"fov\" 40\n"
           "Translate 0 0 5\n"
           "WorldBegin\n"
           "Attribute \"shade\" \"shaderformat\" [\"" +
           lightFormat + "\"]\n" +
           "LightSource \"constl\" 1\n"
           "Attribute \"shade\" \"shaderformat\" [\"" +
           surfaceFormat + "\"]\n" +
           "Surface \"" + surface + "\"\n" +
           "Polygon \"P\" [-50 -50 0  50 -50 0  50 50 0  -50 50 0]\n"
           "WorldEnd\n";
}

int main() {
    printf("GitHub #126: one-argument illuminance(P) under .rslo and .slo, surface x light\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_illum1_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    EXPECT_TRUE(writeFile(dir + "/constl.sl", kLightSource));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/constl.sl", dir + "/constl.rslo", false));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/constl.sl", dir + "/constl.slo", true));
    for (const SurfaceCase &s : surfaces) {
        const std::string src = dir + "/" + s.name + ".sl";
        EXPECT_TRUE(writeFile(src, s.source));
        EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + s.name + ".rslo", false));
        EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + s.name + ".slo", true));
    }

    const char *formats[] = {"rslo", "slo"};
    for (const SurfaceCase &s : surfaces) {
        std::vector<float> reference; // surface .rslo, light .rslo
        for (const char *surfaceFormat : formats) {
            for (const char *lightFormat : formats) {
                const std::string base = dir + "/" + s.name + "_" + surfaceFormat + "_" + lightFormat;
                writeFile(base + ".rib", sceneRib(base + ".tif", lightFormat, surfaceFormat, s.name));
                int sig = 0;
                const RenderEnd end = runOrender(orenderBin, displaysDir, dir, base + ".rib", base + ".log", &sig);

                std::vector<float> img;
                const bool read = end == RENDER_DONE && readImage(base + ".tif", img);
                const bool isReference = std::string(surfaceFormat) == "rslo" && std::string(lightFormat) == "rslo";
                if (isReference && read)
                    reference = img;

                bool same = false;
                if (read && !reference.empty()) {
                    same = true;
                    for (size_t i = 0; i < img.size(); i++)
                        if (!(fabsf(img[i] - reference[i]) < 1e-4f))
                            same = false;
                }
                const size_t centre = 3 * (kSize / 2 * kSize + kSize / 2);
                printf("  %-6s surface .%-4s light .%-4s : %s", s.name, surfaceFormat, lightFormat,
                       end == RENDER_DONE ? (read ? (same ? "ok" : "IMAGE DIFFERS") : "NO IMAGE") : "DID NOT FINISH");
                if (read)
                    printf(" (centre %g %g %g)", img[centre], img[centre + 1], img[centre + 2]);
                printf("\n");
                EXPECT_TRUE(end == RENDER_DONE);
                EXPECT_TRUE(read);
                EXPECT_TRUE(same);
            }
        }

        // The reference must show the light, or "same" proves nothing.
        bool lit = reference.size() == (size_t)kSize * kSize * 3;
        for (size_t i = 0; lit && i < reference.size(); i += 3)
            lit = fabsf(reference[i] - 1) < 1e-4f && fabsf(reference[i + 1] - 2) < 1e-4f && fabsf(reference[i + 2] - 3) < 1e-4f;
        printf("  %-6s reference is the lit (1,2,3) everywhere: %s\n", s.name, lit ? "yes" : "NO");
        EXPECT_TRUE(lit);
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
