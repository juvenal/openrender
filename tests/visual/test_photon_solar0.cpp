/**
 * tests/visual/test_photon_solar0.cpp
 *
 * Photon-map driver for the photon hider's no-argument solar() form
 * (fix/photon-solar0-stale, GitHub #52).
 *
 * CPhotonHider::solarBegin() once did nothing for solar() without a
 * direction, and solarEnd() then traced one photon per vertex from whatever
 * L, Ps and powerScale the shading state held: zeros for the first light
 * (nothing stored), or the preceding solar light's axis, disc and power.
 *
 * Renders three scenes and reads the global photon map each one writes. In
 * all three solar0_light is blue and distantlight red, so a photon's blue
 * power is solar0_light's alone and its red power distantlight's:
 *
 *   alone:  solar0_light only;
 *   after:  distantlight, then solar0_light;
 *   before: solar0_light, then distantlight.
 *
 * Checks:
 *   1. every scene writes a readable map (no map means nothing was stored);
 *   2. every photon's power is finite and non-negative;
 *   3. the alone map holds no red or green power;
 *   4. solar0_light's blue power is the same in all three maps, so it does
 *      not depend on which light ran before it;
 *   5. distantlight's red power is the same in after and before;
 *   6. in every map, at least MIN_UPWARD_SHARE of the blue power was stored
 *      by photons travelling upward (stored normal N.y < -0.5: tracePhoton
 *      flips the normal to face the incoming photon). Photons arriving from
 *      every direction reach the floor's underside and the sphere's lower
 *      half; photons replaying distantlight's downward axis do not.
 *
 * Each map is deleted before its scene renders.
 *
 * Usage:
 *   test_photon_solar0 <orender> <extra_orender_args>
 *                      <alone.rib> <alone.gpm>
 *                      <after.rib> <after.gpm>
 *                      <before.rib> <before.gpm>
 */

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Relative tolerance on the power sums of checks 4 and 5. The three scenes
// draw different stretches of the photon hider's shared sample sequences, so
// the sums agree statistically, not bit for bit.
static constexpr double SUM_TOLERANCE = 0.05;

// Minimum share of solar0_light's blue power stored by upward-travelling
// photons (check 6).
static constexpr double MIN_UPWARD_SHARE = 0.15;

// On-disk layout of one CPhoton (src/ri/hiders/photonMap.h): vector P, N;
// short flags (+2 padding); vector C; unsigned char theta, phi (+2 padding).
static constexpr size_t PHOTON_SIZE = 44;
static constexpr size_t PHOTON_N_OFFSET = 12;
static constexpr size_t PHOTON_C_OFFSET = 28;

struct MapSummary {
        bool ok = false;
        std::string error;
        long numPhotons = 0;
        long numBad = 0;     // photons with a non-finite or negative power
        double sum[3] = {0, 0, 0};
        double blueUpward = 0; // blue power of photons stored with N.y < -0.5
};

static bool readAll(const char *path, std::vector<unsigned char> &bytes) {
    FILE *f = fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    fclose(f);
    return true;
}

template <typename T>
static T load(const std::vector<unsigned char> &b, size_t offset) {
    T v;
    memcpy(&v, b.data() + offset, sizeof(T));
    return v;
}

// File layout: the ropen() header (magic, four version ints, an int length
// and a NUL-terminated type string), then CMap<CPhoton>::write() (numItems,
// maxItems, numItems + 1 photons with item 0 unused, bmin, bmax), then the
// two matrices and maxPower that CPhotonMap::write() appends.
static MapSummary summarize(const char *path) {
    MapSummary s;
    std::vector<unsigned char> b;
    if (!readAll(path, b)) {
        s.error = "no map written (nothing was stored)";
        return s;
    }

    size_t o = 4 + 16;
    if (b.size() < o + 4) {
        s.error = "truncated header";
        return s;
    }
    const int32_t typeLength = load<int32_t>(b, o);
    o += 4 + (size_t)typeLength + 1;
    if (typeLength < 0 || b.size() < o + 8) {
        s.error = "truncated header";
        return s;
    }
    const int32_t numItems = load<int32_t>(b, o);
    o += 8;
    const size_t expected = o + PHOTON_SIZE * ((size_t)numItems + 1) + 6 * 4 + 32 * 4 + 4;
    if (numItems < 0 || b.size() != expected) {
        s.error = "unexpected file size " + std::to_string(b.size()) + " for " +
                  std::to_string(numItems) + " photons (expected " + std::to_string(expected) +
                  "): has the CPhoton layout changed?";
        return s;
    }

    for (int32_t k = 1; k <= numItems; k++) {
        const size_t p = o + PHOTON_SIZE * (size_t)k;
        const float ny = load<float>(b, p + PHOTON_N_OFFSET + 4);
        float c[3];
        bool bad = false;
        for (int i = 0; i < 3; i++) {
            c[i] = load<float>(b, p + PHOTON_C_OFFSET + 4 * (size_t)i);
            if (!std::isfinite(c[i]) || c[i] < 0) {
                bad = true;
            }
        }
        s.numPhotons++;
        if (bad) {
            s.numBad++;
            continue;
        }
        for (int i = 0; i < 3; i++) {
            s.sum[i] += c[i];
        }
        if (ny < -0.5f) {
            s.blueUpward += c[2];
        }
    }
    s.ok = true;
    return s;
}

static int failures = 0;

__attribute__((format(printf, 1, 2))) static void fail(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("  FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    failures++;
}

static bool withinTolerance(double a, double b) {
    return std::fabs(a - b) <= SUM_TOLERANCE * std::fabs(b);
}

int main(int argc, char *argv[]) {
    if (argc != 9) {
        fprintf(stderr,
                "Usage: %s <orender> <extra_orender_args> <alone.rib> <alone.gpm> "
                "<after.rib> <after.gpm> <before.rib> <before.gpm>\n",
                argv[0]);
        return 1;
    }
    const char *orender = argv[1];
    const char *extra = argv[2];
    const char *names[3] = {"alone", "after", "before"};
    MapSummary maps[3];

    for (int m = 0; m < 3; m++) {
        const char *rib = argv[3 + 2 * m];
        const char *map = argv[4 + 2 * m];
        remove(map);
        std::string cmd = std::string(orender);
        if (extra[0] != '\0') {
            cmd += " ";
            cmd += extra;
        }
        cmd += " \"" + std::string(rib) + "\"";
        printf("Running: %s\n", cmd.c_str());
        if (system(cmd.c_str()) != 0) {
            fprintf(stderr, "  orender failed on %s\n", rib);
            return 1;
        }
        maps[m] = summarize(map);
        const MapSummary &s = maps[m];
        if (s.ok) {
            printf("  %-6s %s: %ld photons, %ld bad, power (%.5g, %.5g, %.5g), upward blue share %.4f\n",
                   names[m], map, s.numPhotons, s.numBad, s.sum[0], s.sum[1], s.sum[2],
                   s.sum[2] > 0 ? s.blueUpward / s.sum[2] : 0.0);
        }
        else {
            printf("  %-6s %s: %s\n", names[m], map, s.error.c_str());
        }
    }

    // 1, 2, 6
    for (int m = 0; m < 3; m++) {
        const MapSummary &s = maps[m];
        if (!s.ok) {
            fail("%s: %s", names[m], s.error.c_str());
            continue;
        }
        if (s.numBad > 0) {
            fail("%s: %ld of %ld photons have a non-finite or negative power", names[m], s.numBad,
                 s.numPhotons);
        }
        const double share = s.sum[2] > 0 ? s.blueUpward / s.sum[2] : 0.0;
        if (share < MIN_UPWARD_SHARE) {
            fail("%s: upward share of solar0_light's power %.4f is below %.2f", names[m], share,
                 MIN_UPWARD_SHARE);
        }
    }

    // 3
    if (maps[0].ok && (maps[0].sum[0] != 0 || maps[0].sum[1] != 0)) {
        fail("alone: red/green power (%.5g, %.5g) without a red light", maps[0].sum[0], maps[0].sum[1]);
    }

    // 4
    const double blueRef = maps[0].ok ? maps[0].sum[2] : 0.0;
    for (int m = 1; m < 3; m++) {
        if (!maps[m].ok) {
            continue;
        }
        if (!maps[0].ok || blueRef <= 0 || !withinTolerance(maps[m].sum[2], blueRef)) {
            fail("%s: solar0_light's power %.5g differs from its power alone %.5g", names[m],
                 maps[m].sum[2], blueRef);
        }
    }

    // 5
    if (maps[1].ok && maps[2].ok && !withinTolerance(maps[1].sum[0], maps[2].sum[0])) {
        fail("after: distantlight's power %.5g differs from its power before solar0_light %.5g",
             maps[1].sum[0], maps[2].sum[0]);
    }

    if (failures > 0) {
        printf("FAIL (%d check%s)\n", failures, failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
