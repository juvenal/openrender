/**
 * Project: openRender
 *
 * File: tests/arealight/test_vocabulary_mirror.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), State_AreaLightVocabularyMirror (contracts/tests.md "Added at the tasks stage";
 *   tasks.md T057), in the style of State_VocabularyConsistency (tests/unit/state): Graphics State's EAreaLightMode
 *   (state/stateVocabulary.h) and Shading's openrender::shading::AreaLightMode (libshader/shading/areaLightSampler.h) are two
 *   enums so that neither domain includes the other's header; they must carry the same values, value for value, and the same
 *   size. A compile-time check and a run-time one (the run-time one names the first pair that drifts).
 *
 *   Red on the starting tree: does not compile (areaLightSampler.h does not exist).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include <cstdio>

#include "libshader/shading/areaLightSampler.h"
#include "state/stateVocabulary.h"

using openrender::shading::AreaLightMode;

#define MIRROR(name) static_assert(static_cast<int>(AreaLightMode::name) == static_cast<int>(EAreaLightMode::name), "AreaLightMode::" #name " drifted from EAreaLightMode")
MIRROR(Unset);
MIRROR(Full);
MIRROR(Fast);
MIRROR(Off);
static_assert(sizeof(AreaLightMode) == sizeof(EAreaLightMode), "AreaLightMode and EAreaLightMode differ in size");

int main() {
    struct Pair {
            const char *name;
            int shading, state;
    };
    const Pair pairs[] = {
        {"Unset", static_cast<int>(AreaLightMode::Unset), static_cast<int>(EAreaLightMode::Unset)},
        {"Full", static_cast<int>(AreaLightMode::Full), static_cast<int>(EAreaLightMode::Full)},
        {"Fast", static_cast<int>(AreaLightMode::Fast), static_cast<int>(EAreaLightMode::Fast)},
        {"Off", static_cast<int>(AreaLightMode::Off), static_cast<int>(EAreaLightMode::Off)},
    };
    int failures = 0;
    for (const Pair &p : pairs)
        if (p.shading != p.state) {
            fprintf(stderr, "  FAIL AreaLightMode::%s = %d but EAreaLightMode::%s = %d\n", p.name, p.shading, p.name, p.state);
            ++failures;
        }
    // the values themselves are part of the contract (data-model.md 1): Unset 0, Full 1, Fast 2, Off 3
    if (static_cast<int>(AreaLightMode::Unset) != 0 || static_cast<int>(AreaLightMode::Full) != 1 || static_cast<int>(AreaLightMode::Fast) != 2 ||
        static_cast<int>(AreaLightMode::Off) != 3) {
        fprintf(stderr, "  FAIL the values are not Unset 0, Full 1, Fast 2, Off 3\n");
        ++failures;
    }
    printf("State_AreaLightVocabularyMirror: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
