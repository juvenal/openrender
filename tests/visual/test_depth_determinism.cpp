/**
 * tests/visual/test_depth_determinism.cpp
 *
 * Output determinism driver (fix/stochastic-depthfilter-race, GitHub #48).
 *
 * Renders one scene <runs> times and checks its 32-bit float output, either a
 * one-channel "z" display or any float colour display ("quantize" [0 0 0 0]):
 *
 *   1. For z only: no pixel reads above C_INFINITY. A pixel with no visible surface must
 *      read exactly C_INFINITY, whichever path produced it: the reyes hider
 *      writes it directly for a bucket with nothing queued, and the
 *      average/midpoint depth filters must give the same value for a bucket
 *      whose subsamples are all background (summing sixteen C_INFINITY
 *      subsamples in float used to land one rounding step above it).
 *   2. Every run is bit-identical to the first, every channel. Which buckets
 *      have objects queued depends on thread timing, so (1) failing shows up
 *      here as run-to-run differences in multi-threaded renders. For colour,
 *      the order in which two fragments at the same depth reached a pixel
 *      used to depend on thread timing too: a sample on a micropolygon edge
 *      shared by two grids was counted by both (#50), and compositing the
 *      pair in either order moved the colour by one float ULP (#48).
 *
 * Usage:
 *   test_depth_determinism <orender> <rib> <output_tif> <runs> [orender options...]
 *
 * The scene's Display must write <output_tif> as 32-bit float in the working
 * directory.
 *
 * Cross-configuration mode (spec 039, FR-030): the scene is rendered once per orender
 * configuration and every listed output is compared bit for bit with the first
 * configuration's, at any bit depth.
 *
 *   test_depth_determinism --configs "<opts>|<opts>|..." --outputs <a.tif>[,<b.tif>...]
 *       [--prepend "<RIB line>"] [--config-prepends "<RIB line>|<RIB line>|..."]
 *       [--no-dither] [--keep-pins] <orender> <rib>
 *   test_depth_determinism --print-scene [--prepend ...] [--no-dither] [--keep-pins] <orender> <rib>
 *
 *   --configs      orender options per configuration, separated by '|'. An empty entry is the
 *                  default thread count. A `-P:n` entry must not carry `-t`.
 *   --prepend      first line of a temporary copy of the scene (written to the working
 *                  directory, so relative archive paths resolve as they do for the original).
 *                  The copy drops every line that mentions `numthreads`, so a pinned scene
 *                  really runs multithreaded, unless --keep-pins is given.
 *   --config-prepends  one RIB line per configuration, in --configs order, put after the
 *                  --prepend line in that configuration's own copy of the scene. The
 *                  configurations then differ by scene content as well as by options (for
 *                  example by `Option "sampling" "seed"`), which is how the harness is shown to
 *                  catch a real, deterministic difference.
 *   --no-dither    puts `Quantize "rgba" 255 0 255 0` after the --prepend line (a scene's own
 *                  later Quantize still wins).
 *   --print-scene  prints the scene that would be rendered and exits 0 without rendering.
 *
 * Exit codes: 0 every output identical, 1 a difference (at most 5 differing pixels are
 * listed per output), 2 a render, decode or usage failure.
 */

#include "depth_tiff.h"
#include "scene_copy.h"

// Check (1): count pixels above C_INFINITY or NaN, report the first few.
static int checkBackground(const DepthImage &img, int run) {
    int bad = 0;
    for (size_t i = 0; i < img.z.size(); i++) {
        const float z = img.z[i];
        if (!(z <= RENDERER_INFINITY)) {
            if (bad < 5)
                fprintf(stderr, "  run %d: pixel (%zu, %zu) z = %.9g, above C_INFINITY (%.9g)\n", run,
                        i % img.width, i / img.width, z, RENDERER_INFINITY);
            bad++;
        }
    }
    return bad;
}

static int legacyMain(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <orender> <rib> <output_tif> <runs> [orender options...]\n", argv[0]);
        return 1;
    }
    const char *orender = argv[1];
    const char *rib = argv[2];
    const char *outputTif = argv[3];
    const int runs = atoi(argv[4]);
    if (runs < 1) {
        fprintf(stderr, "  runs must be at least 1\n");
        return 1;
    }

    std::string command = std::string("\"") + orender + "\"";
    for (int i = 5; i < argc; i++)
        command += std::string(" ") + argv[i];
    command += std::string(" \"") + rib + "\"";
    printf("Running %d time(s): %s\n", runs, command.c_str());

    DepthImage first;
    int failures = 0;
    for (int run = 1; run <= runs; run++) {
        DepthImage img;
        if (!renderScene(command, outputTif) || !readFloatTiff(outputTif, img))
            return 1;

        const int above = (img.channels == 1) ? checkBackground(img, run) : 0;
        if (above > 0) {
            fprintf(stderr, "  FAIL run %d: %d pixel(s) above C_INFINITY\n", run, above);
            failures++;
        }

        if (run == 1) {
            first = std::move(img);
            continue;
        }
        if (img.width != first.width || img.height != first.height || img.channels != first.channels) {
            fprintf(stderr, "  FAIL run %d: size %ux%ux%u, run 1 was %ux%ux%u\n", run, img.width, img.height,
                    img.channels, first.width, first.height, first.channels);
            failures++;
            continue;
        }
        int differing = 0;
        for (size_t i = 0; i < img.z.size(); i++) {
            if (memcmp(&img.z[i], &first.z[i], sizeof(float)) != 0) {
                if (differing < 5)
                    fprintf(stderr, "  run %d: pixel (%zu, %zu) channel %zu = %.9g, run 1 had %.9g\n", run,
                            (i / img.channels) % img.width, (i / img.channels) / img.width, i % img.channels,
                            img.z[i], first.z[i]);
                differing++;
            }
        }
        if (differing > 0) {
            fprintf(stderr, "  FAIL run %d: %d value(s) differ from run 1\n", run, differing);
            failures++;
        }
    }

    if (failures > 0)
        return 1;
    printf("  PASS: %d run(s)%s%s\n", runs, first.channels == 1 ? ", no pixel above C_INFINITY" : "",
           runs > 1 ? ", all bit-identical to run 1" : "");
    return 0;
}

// --- Cross-configuration mode -------------------------------------------------------------

namespace {

    constexpr int kExitDifferent = 1;
    constexpr int kExitFailure = 2;
    constexpr int kMaxPixelsPerOutput = 5;

    struct SweepOptions {
            std::vector<std::string> configs;
            std::vector<std::string> outputs;
            std::vector<std::string> configPrepends;
            SceneEdit edit;
            bool printScene = false;
            std::string orender;
            std::string rib;
    };

    std::vector<std::string> splitOn(const std::string &s, char sep) {
        std::vector<std::string> parts;
        size_t start = 0;
        for (;;) {
            const size_t at = s.find(sep, start);
            if (at == std::string::npos) {
                parts.push_back(s.substr(start));
                return parts;
            }
            parts.push_back(s.substr(start, at - start));
            start = at + 1;
        }
    }

    std::string configLabel(const std::string &config) { return config.empty() ? "(default)" : config; }

    // One sample (or one byte, for a depth that is not a whole number of bytes) as text.
    std::string unitText(const RawTiff &img, const uint8_t *p) {
        char buf[64];
        const uint16_t bits = img.bitsPerSample;
        if (bits % 8 != 0) {
            snprintf(buf, sizeof(buf), "0x%02x", p[0]);
        }
        else if (bits == 32 && img.sampleFormat == SAMPLEFORMAT_IEEEFP) {
            float v;
            memcpy(&v, p, 4);
            snprintf(buf, sizeof(buf), "%.9g", v);
        }
        else if (bits == 64 && img.sampleFormat == SAMPLEFORMAT_IEEEFP) {
            double v;
            memcpy(&v, p, 8);
            snprintf(buf, sizeof(buf), "%.17g", v);
        }
        else if (bits == 8 || bits == 16 || bits == 32) {
            const bool isSigned = img.sampleFormat == SAMPLEFORMAT_INT;
            uint32_t u = 0;
            memcpy(&u, p, bits / 8); // little-endian hosts only, like the rest of the suite
            if (isSigned)
                snprintf(buf, sizeof(buf), "%d", bits == 8 ? (int)(int8_t)u : bits == 16 ? (int)(int16_t)u
                                                                                         : (int)u);
            else
                snprintf(buf, sizeof(buf), "%u", u);
        }
        else {
            std::string hex = "0x";
            for (int i = (int)(bits / 8) - 1; i >= 0; i--) {
                snprintf(buf, sizeof(buf), "%02x", p[i]);
                hex += buf;
            }
            return hex;
        }
        return buf;
    }

    // Compares two decoded outputs. Prints up to `budget` differing pixels (decremented as it
    // prints) and returns how many values differ; -1 if the layouts differ.
    long compareRaw(const RawTiff &a, const RawTiff &b, const std::string &name, const std::string &aLabel, const std::string &bLabel, int &budget) {
        const std::string header = "  " + name + ", configuration " + aLabel + " against " + bLabel + ":\n";
        if (a.width != b.width || a.height != b.height || a.spp != b.spp || a.bitsPerSample != b.bitsPerSample ||
            a.sampleFormat != b.sampleFormat || a.planar != b.planar) {
            fprintf(stderr, "  FAIL %s: %s is %ux%u, %u channel(s), %u bits, format %u; %s is %ux%u, %u channel(s), %u bits, format %u\n",
                    name.c_str(), aLabel.c_str(), a.width, a.height, a.spp, a.bitsPerSample, a.sampleFormat,
                    bLabel.c_str(), b.width, b.height, b.spp, b.bitsPerSample, b.sampleFormat);
            return -1;
        }
        if (a.data == b.data)
            return 0;

        const bool whole = a.bitsPerSample % 8 == 0;
        const size_t unitBytes = whole ? a.bitsPerSample / 8 : 1;
        const size_t unitsPerRow = a.rowBytes / unitBytes;
        const bool separate = a.planar == PLANARCONFIG_SEPARATE;
        long differing = 0;
        for (size_t plane = 0; plane < a.planes(); plane++) {
            for (uint32_t y = 0; y < a.height; y++) {
                const size_t rowAt = (plane * a.height + y) * a.rowBytes;
                for (size_t u = 0; u < unitsPerRow; u++) {
                    const uint8_t *pa = &a.data[rowAt + u * unitBytes];
                    const uint8_t *pb = &b.data[rowAt + u * unitBytes];
                    if (memcmp(pa, pb, unitBytes) == 0)
                        continue;
                    differing++;
                    if (budget <= 0)
                        continue;
                    if (budget == kMaxPixelsPerOutput)
                        fputs(header.c_str(), stderr);
                    budget--;
                    size_t x, channel;
                    if (whole) {
                        x = separate ? u : u / a.spp;
                        channel = separate ? plane : u % a.spp;
                    }
                    else {
                        x = (u * 8) / ((size_t)a.bitsPerSample * (separate ? 1 : a.spp));
                        channel = separate ? plane : 0;
                    }
                    fprintf(stderr, "    pixel (%zu, %u) channel %zu = %s, %s had %s\n", x, y, channel,
                            unitText(a, pa).c_str(), bLabel.c_str(), unitText(b, pb).c_str());
                }
            }
        }
        return differing;
    }

    int sweepMain(int argc, char *argv[]) {
        SweepOptions o;
        std::vector<std::string> positional;
        for (int i = 1; i < argc; i++) {
            const std::string arg = argv[i];
            auto value = [&](const char *flag) -> const char * {
                if (i + 1 >= argc) {
                    fprintf(stderr, "  %s expects a value\n", flag);
                    return nullptr;
                }
                return argv[++i];
            };
            if (arg == "--configs" || arg == "--outputs" || arg == "--prepend" || arg == "--config-prepends") {
                const char *v = value(arg.c_str());
                if (!v)
                    return kExitFailure;
                if (arg == "--configs")
                    o.configs = splitOn(v, '|');
                else if (arg == "--outputs")
                    o.outputs = splitOn(v, ',');
                else if (arg == "--config-prepends")
                    o.configPrepends = splitOn(v, '|');
                else {
                    o.edit.prepend = v;
                    o.edit.havePrepend = true;
                }
            }
            else if (arg == "--no-dither") {
                o.edit.noDither = true;
            }
            else if (arg == "--keep-pins") {
                o.edit.keepPins = true;
            }
            else if (arg == "--print-scene") {
                o.printScene = true;
            }
            else if (arg.size() > 2 && arg.compare(0, 2, "--") == 0) {
                fprintf(stderr, "  Unknown option %s\n", arg.c_str());
                return kExitFailure;
            }
            else {
                positional.push_back(arg);
            }
        }
        if (positional.size() != 2) {
            fprintf(stderr,
                    "Usage: %s --configs \"<opts>|<opts>...\" --outputs <a.tif>[,<b.tif>...] [--prepend \"<RIB line>\"]\n"
                    "          [--config-prepends \"<RIB line>|...\"] [--no-dither] [--keep-pins] <orender> <rib>\n"
                    "       %s --print-scene [--prepend ...] [--no-dither] [--keep-pins] <orender> <rib>\n",
                    argv[0], argv[0]);
            return kExitFailure;
        }
        o.orender = positional[0];
        o.rib = positional[1];

        if (o.printScene) {
            std::string scene;
            if (!prepareSceneText(o.rib, o.edit, scene))
                return kExitFailure;
            fputs(scene.c_str(), stdout);
            return 0;
        }
        if (o.configs.empty() || o.outputs.empty() || o.outputs[0].empty()) {
            fprintf(stderr, "  --configs and --outputs are required\n");
            return kExitFailure;
        }

        if (!o.configPrepends.empty() && o.configPrepends.size() != o.configs.size()) {
            fprintf(stderr, "  --config-prepends has %zu line(s) for %zu configuration(s)\n", o.configPrepends.size(), o.configs.size());
            return kExitFailure;
        }

        std::string ribToRender = o.rib;
        std::string copyPath;
        if (o.edit.needsCopy() && o.configPrepends.empty()) {
            copyPath = "determinism-" + sceneBaseName(o.rib);
            if (!writeSceneCopy(o.rib, o.edit, copyPath))
                return kExitFailure;
            ribToRender = copyPath;
        }

        std::vector<RawTiff> reference;
        std::vector<int> budget(o.outputs.size(), kMaxPixelsPerOutput);
        bool different = false;
        for (size_t c = 0; c < o.configs.size(); c++) {
            if (!o.configPrepends.empty()) {
                // This configuration's own copy: the --prepend line, then its line
                SceneEdit edit = o.edit;
                edit.prepend = (o.edit.havePrepend ? o.edit.prepend + "\n" : std::string()) + o.configPrepends[c];
                edit.havePrepend = true;
                copyPath = "determinism-" + std::to_string(c + 1) + "-" + sceneBaseName(o.rib);
                if (!writeSceneCopy(o.rib, edit, copyPath))
                    return kExitFailure;
                ribToRender = copyPath;
            }
            std::string command = "\"" + o.orender + "\"";
            if (!o.configs[c].empty())
                command += " " + o.configs[c];
            command += " \"" + ribToRender + "\"";
            printf("Configuration %zu of %zu %s: %s\n", c + 1, o.configs.size(), configLabel(o.configs[c]).c_str(),
                   command.c_str());
            fflush(stdout);

            for (const std::string &output : o.outputs)
                remove(output.c_str());
            const int rc = system(command.c_str());
            if (rc != 0) {
                fprintf(stderr, "  orender failed with exit code %d for configuration %s\n", rc,
                        configLabel(o.configs[c]).c_str());
                return kExitFailure;
            }

            std::vector<RawTiff> images(o.outputs.size());
            for (size_t k = 0; k < o.outputs.size(); k++) {
                struct stat st;
                if (stat(o.outputs[k].c_str(), &st) != 0) {
                    fprintf(stderr, "  Output TIF not found: %s (configuration %s)\n", o.outputs[k].c_str(),
                            configLabel(o.configs[c]).c_str());
                    return kExitFailure;
                }
                if (!readRawTiff(o.outputs[k].c_str(), images[k]))
                    return kExitFailure;
            }
            if (c == 0) {
                reference = std::move(images);
                continue;
            }
            for (size_t k = 0; k < o.outputs.size(); k++) {
                const long n = compareRaw(images[k], reference[k], o.outputs[k], configLabel(o.configs[c]),
                                          configLabel(o.configs[0]), budget[k]);
                if (n != 0) {
                    different = true;
                    if (n > 0) {
                        fprintf(stderr, "  FAIL %s: %ld value(s) differ between configurations %s and %s\n",
                                o.outputs[k].c_str(), n, configLabel(o.configs[c]).c_str(),
                                configLabel(o.configs[0]).c_str());
                    }
                }
            }
        }

        if (different) {
            if (copyPath.empty())
                fprintf(stderr, "  DIFFERENT\n");
            else
                fprintf(stderr, "  DIFFERENT (the scene as rendered is kept in %s)\n", copyPath.c_str());
            return kExitDifferent;
        }
        for (size_t k = 0; k < o.outputs.size(); k++) {
            const RawTiff &r = reference[k];
            printf("  %s: %ux%u, %u channel(s), %u-bit, bit-identical across %zu configuration(s)\n",
                   o.outputs[k].c_str(), r.width, r.height, r.spp, r.bitsPerSample, o.configs.size());
        }
        printf("  PASS: %zu output(s), %zu configuration(s)\n", o.outputs.size(), o.configs.size());
        for (size_t c = 0; c < o.configPrepends.size(); c++)
            remove(("determinism-" + std::to_string(c + 1) + "-" + sceneBaseName(o.rib)).c_str());
        if (!copyPath.empty())
            remove(copyPath.c_str());
        return 0;
    }

} // namespace

int main(int argc, char *argv[]) {
    if (argc > 1 && strncmp(argv[1], "--", 2) == 0)
        return sweepMain(argc, argv);
    return legacyMain(argc, argv);
}
