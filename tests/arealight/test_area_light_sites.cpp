/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_light_sites.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaLight_SamplingSites (contracts/tests.md; FR-008; data-model.md 7). tasks.md T023.
 *
 *   The renderer-side sampler call-site constants are pairwise distinct, all at or above 0xFF00, and above the
 *   compiler's call-site ordinals. The compiler numbers a shader's stochastic calls 0, 1, 2 ... (CCallSitePass), so the
 *   ordinals stay far below 0xFF00 = 65280 stochastic calls in one shader; the renderer-side sites live above it so a
 *   shader's own stream never collides with the renderer's. 057 owns the range 0xFF20-0xFF2F: the three constants
 *   kAreaLightSampleSite 0xFF20, kAreaLightShadowSite 0xFF21 and kAreaLightPhotonSite 0xFF22, which neither the photon
 *   hider's sites (kPhotonEmitSite 0xFF11 and kPhotonTraceSite 0xFF12, file-local constants of photon.cpp, named here by
 *   value) nor spec 040 may reuse.
 *
 *   Red on the starting tree: does not compile (the 057 constants do not exist).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <cstdint>
#include <cstdio>
#include <set>

#include "common/sampling.h"

namespace sp = openrender::sampling;

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

// The photon hider's sites, from photon.cpp:64-65 (file-local there)
static constexpr uint32_t kPhotonEmitSite = 0xFF11;
static constexpr uint32_t kPhotonTraceSite = 0xFF12;

// The compiler numbers a shader's stochastic calls from 0; a shader with this many of them would reach the renderer's sites.
static constexpr uint32_t kRendererSiteFloor = 0xFF00;

static_assert(sp::kAreaLightSampleSite == 0xFF20, "emitter point draws");
static_assert(sp::kAreaLightShadowSite == 0xFF21, "shadow ray keys");
static_assert(sp::kAreaLightPhotonSite == 0xFF22, "photon emission points of a bound light");

int main() {
    printf("AreaLight_SamplingSites (FR-008)\n");

    const uint32_t sites[] = {kPhotonEmitSite, kPhotonTraceSite, sp::kAreaLightSampleSite, sp::kAreaLightShadowSite, sp::kAreaLightPhotonSite};
    std::set<uint32_t> distinct(sites, sites + sizeof(sites) / sizeof(sites[0]));
    CHECK(distinct.size() == sizeof(sites) / sizeof(sites[0]), "the five renderer-side sites are pairwise distinct");

    for (uint32_t s : sites)
        CHECK(s >= kRendererSiteFloor, "every renderer-side site is at or above 0xFF00, above any compiler ordinal");

    const uint32_t ours[] = {sp::kAreaLightSampleSite, sp::kAreaLightShadowSite, sp::kAreaLightPhotonSite};
    for (uint32_t s : ours)
        CHECK(s >= 0xFF20 && s <= 0xFF2F, "057's sites lie in its reserved range 0xFF20-0xFF2F");
    for (uint32_t s : {kPhotonEmitSite, kPhotonTraceSite})
        CHECK(s < 0xFF20 || s > 0xFF2F, "the photon hider's sites lie outside 057's reserved range");

    // the sites fit the 16-bit call-site parameter of the key functions
    for (uint32_t s : sites)
        CHECK(s <= 0xFFFF, "every site fits the uint16_t call-site argument of childKey/substream");

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
