/**
 * Project: openRender
 *
 * File: tests/arealight/test_controls_resolve.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), ShadingContract_AreaLightControls (contracts/tests.md; FR-018; US5). tasks.md T079.
 *
 *   resolveAreaLightControls() (libshader/shading/areaLightControls.h) is the one function that combines the three levels of the
 *   "arealight" controls for a receiver and a bound light. No renderer, no shading context: a pure function.
 *
 *   1. The precedence table, exhaustive: every combination of {object, light, scene} setting or not `samples` (unset = 0) and `mode`
 *      (Unset, Full, Fast, Off): 2^3 sample settings x 4^3 mode settings = 512 cases. The expected value of each is
 *      derived in this file by a different route than the header's (a ranked scan over the levels), and a hand-written list of
 *      cases pins both against the stated rule.
 *   2. Fast: n in {1, 2, 3, 4, 5, 16, 17, 64} gives max(1, ceil(n / 4)) = {1, 1, 1, 1, 2, 4, 5, 16}.
 *   3. Off wins whatever the others say, at any level; `samples` and `mode` resolve independently (a more specific level that sets
 *      only one of them leaves the other to the next level).
 *   4. An all-unset chain gives samples == 0 (iterateLights treats it as "skip"; the hider always fills the scene level, so a
 *      real frame never sees it), and the mode is Full.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include <cstdio>

#include "libshader/shading/areaLightControls.h"

using namespace openrender::shading;

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, label)                                                                  \
    do {                                                                                    \
        if (cond)                                                                           \
            ++g_passed;                                                                     \
        else {                                                                              \
            ++g_failed;                                                                     \
            fprintf(stderr, "  FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__, #cond, label); \
        }                                                                                   \
    } while (0)

static const AreaLightMode kModes[4] = {AreaLightMode::Unset, AreaLightMode::Full, AreaLightMode::Fast, AreaLightMode::Off};
static const char *modeName(AreaLightMode m) {
    switch (m) {
        case AreaLightMode::Unset: return "unset";
        case AreaLightMode::Full: return "full";
        case AreaLightMode::Fast: return "fast";
        case AreaLightMode::Off: return "off";
    }
    return "?";
}

// The rule written the other way round: levels listed most specific first, and a scan for the first one that sets each control
struct Level {
        int samples;
        AreaLightMode mode;
};
static AreaLightControls expected(const Level object, const Level light, const Level scene) {
    const Level chain[3] = {object, light, scene};
    AreaLightControls r{0, AreaLightMode::Full};
    for (const Level &l : chain)
        if (l.samples > 0) {
            r.samples = l.samples;
            break;
        }
    bool off = false;
    for (const Level &l : chain)
        off = off || l.mode == AreaLightMode::Off;
    if (off) {
        r.mode = AreaLightMode::Off;
    }
    else {
        for (const Level &l : chain)
            if (l.mode != AreaLightMode::Unset) {
                r.mode = l.mode;
                break;
            }
    }
    if (r.mode == AreaLightMode::Fast && r.samples > 0) {
        int q = r.samples / 4;
        if (r.samples % 4 != 0)
            ++q;
        r.samples = q < 1 ? 1 : q;
    }
    return r;
}

static AreaLightControls resolve(const Level object, const Level light, const Level scene) {
    AreaLightInfo info;
    info.bound = true;
    info.lightSamples = light.samples;
    info.lightMode = light.mode;
    info.sceneSamples = scene.samples;
    info.sceneMode = scene.mode;
    info.totalArea = 4.0;
    return resolveAreaLightControls(object.samples, object.mode, info);
}

int main() {
    printf("ShadingContract_AreaLightControls (FR-018)\n");

    // ---- 1. the exhaustive table: 8 sample settings x 64 mode settings ----------------------------------------------------
    const int kObject = 3, kLight = 5, kScene = 7; // distinct, so the result names the level that won
    int cases = 0;
    for (int sMask = 0; sMask < 8; sMask++)
        for (int a = 0; a < 4; a++)
            for (int b = 0; b < 4; b++)
                for (int c = 0; c < 4; c++) {
                    const Level object{(sMask & 1) ? kObject : 0, kModes[a]};
                    const Level light{(sMask & 2) ? kLight : 0, kModes[b]};
                    const Level scene{(sMask & 4) ? kScene : 0, kModes[c]};
                    const AreaLightControls got = resolve(object, light, scene);
                    const AreaLightControls want = expected(object, light, scene);
                    ++cases;
                    if (got.samples != want.samples || got.mode != want.mode) {
                        CHECK(false, "table case");
                        fprintf(stderr, "    object (%d, %s) light (%d, %s) scene (%d, %s): got (%d, %s), want (%d, %s)\n", object.samples, modeName(object.mode),
                                light.samples, modeName(light.mode), scene.samples, modeName(scene.mode), got.samples, modeName(got.mode), want.samples,
                                modeName(want.mode));
                    }
                    else
                        ++g_passed;
                }
    CHECK(cases == 512, "the table is exhaustive: 8 x 64 cases");

    // ---- hand-written cases (the stated rule, not derived) ------------------------------------------------------------------
    {
        AreaLightControls r = resolve({64, AreaLightMode::Unset}, {8, AreaLightMode::Unset}, {4, AreaLightMode::Unset});
        CHECK(r.samples == 64 && r.mode == AreaLightMode::Full, "object samples win over light and scene");
        r = resolve({0, AreaLightMode::Unset}, {8, AreaLightMode::Unset}, {4, AreaLightMode::Unset});
        CHECK(r.samples == 8, "light samples win over scene");
        r = resolve({0, AreaLightMode::Unset}, {0, AreaLightMode::Unset}, {4, AreaLightMode::Unset});
        CHECK(r.samples == 4 && r.mode == AreaLightMode::Full, "the scene level alone: its samples, mode Full (the default)");
        r = resolve({0, AreaLightMode::Fast}, {0, AreaLightMode::Full}, {16, AreaLightMode::Full});
        CHECK(r.samples == 4 && r.mode == AreaLightMode::Fast, "object mode Fast over light and scene Full: 16 -> 4");
        r = resolve({0, AreaLightMode::Full}, {0, AreaLightMode::Off}, {16, AreaLightMode::Full});
        CHECK(r.mode == AreaLightMode::Off, "Off at the light level wins over an object that asks for Full");
        r = resolve({0, AreaLightMode::Off}, {0, AreaLightMode::Full}, {16, AreaLightMode::Full});
        CHECK(r.mode == AreaLightMode::Off, "Off at the object level wins");
        r = resolve({0, AreaLightMode::Full}, {0, AreaLightMode::Full}, {16, AreaLightMode::Off});
        CHECK(r.mode == AreaLightMode::Off, "Off at the scene level wins");
        // independence: the object sets only the mode, the light only the count
        r = resolve({0, AreaLightMode::Fast}, {32, AreaLightMode::Unset}, {16, AreaLightMode::Full});
        CHECK(r.samples == 8 && r.mode == AreaLightMode::Fast, "object mode with the light's samples: 32 fast -> 8");
        r = resolve({0, AreaLightMode::Unset}, {0, AreaLightMode::Unset}, {0, AreaLightMode::Unset});
        CHECK(r.samples == 0 && r.mode == AreaLightMode::Full, "an all-unset chain: no samples (skip), mode Full");
    }

    // ---- 2. Fast: max(1, ceil(n / 4)) ---------------------------------------------------------------------------------------
    {
        const int n[8] = {1, 2, 3, 4, 5, 16, 17, 64};
        const int want[8] = {1, 1, 1, 1, 2, 4, 5, 16};
        for (int i = 0; i < 8; i++) {
            const AreaLightControls r = resolve({0, AreaLightMode::Unset}, {0, AreaLightMode::Unset}, {n[i], AreaLightMode::Fast});
            CHECK(r.mode == AreaLightMode::Fast && r.samples == want[i], "Fast count = max(1, ceil(n / 4))");
            const AreaLightControls full = resolve({0, AreaLightMode::Unset}, {0, AreaLightMode::Unset}, {n[i], AreaLightMode::Full});
            CHECK(full.samples == n[i], "Full keeps n");
        }
    }

    printf("%d checks passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
