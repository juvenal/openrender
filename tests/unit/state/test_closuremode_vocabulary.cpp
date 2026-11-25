/**
 * Project: openRender
 *
 * File: tests/unit/state/test_closuremode_vocabulary.cpp
 *
 * Description:
 *   Spec 035 (closure tree and BSDF), Phase 3 (T057), State_ClosureModeVocabularyMirror (labels state;closuremode), in the style of
 *   State_AreaLightVocabularyMirror: Graphics State's EClosureMode (state/stateVocabulary.h) and Shading's
 *   openrender::shading::ClosureMode (libshader/closures/closureMode.h) are two enums so that neither domain includes the other's header;
 *   they must carry the same values, value for value, and the same size: Unset 0, Color 1, Both 2, Closures 3.
 *
 *   Red on the starting tree: does not compile (closureMode.h does not exist, nor does EClosureMode).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include <cstdio>

#include "libshader/closures/closureMode.h"
#include "ri/state/stateVocabulary.h"

using openrender::shading::ClosureMode;

#define MIRROR(name) static_assert(static_cast<int>(ClosureMode::name) == static_cast<int>(EClosureMode::name), "ClosureMode::" #name " drifted from EClosureMode")
MIRROR(Unset);
MIRROR(Color);
MIRROR(Both);
MIRROR(Closures);
static_assert(sizeof(ClosureMode) == sizeof(EClosureMode), "ClosureMode and EClosureMode differ in size");

int main() {
    struct Pair {
            const char *name;
            int shading, state;
    };
    const Pair pairs[] = {
        {"Unset", static_cast<int>(ClosureMode::Unset), static_cast<int>(EClosureMode::Unset)},
        {"Color", static_cast<int>(ClosureMode::Color), static_cast<int>(EClosureMode::Color)},
        {"Both", static_cast<int>(ClosureMode::Both), static_cast<int>(EClosureMode::Both)},
        {"Closures", static_cast<int>(ClosureMode::Closures), static_cast<int>(EClosureMode::Closures)},
    };
    int failures = 0;
    for (const Pair &p : pairs)
        if (p.shading != p.state) {
            fprintf(stderr, "  FAIL ClosureMode::%s = %d but EClosureMode::%s = %d\n", p.name, p.shading, p.name, p.state);
            ++failures;
        }
    // the values themselves are part of the contract (data-model.md): Unset 0, Color 1, Both 2, Closures 3
    if (static_cast<int>(ClosureMode::Unset) != 0 || static_cast<int>(ClosureMode::Color) != 1 || static_cast<int>(ClosureMode::Both) != 2 ||
        static_cast<int>(ClosureMode::Closures) != 3) {
        fprintf(stderr, "  FAIL the values are not Unset 0, Color 1, Both 2, Closures 3\n");
        ++failures;
    }
    printf("State_ClosureModeVocabularyMirror: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
