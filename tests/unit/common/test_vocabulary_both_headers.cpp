// Spec 023 (FR-004): the public ri.h and the common vocabulary header declare
// the same RI types and tokens. Including both in one translation unit must
// compile (identical typedef/macro redefinitions are legal; conflicting ones
// are errors), and both declarations must name the same objects.

#include <cstdio>
#include <cstring>
#include <type_traits>

#include "common/riVocabulary.h"
#include "ri.h"

static_assert(std::is_same_v<RtToken, const char *>, "RtToken is const char *");
static_assert(sizeof(RtBasis) == 16 * sizeof(RtFloat), "RtBasis is a 4x4 float matrix");

int main() {
    int failures = 0;
    // One spelling, one definition: both headers resolve to riTokens.cpp.
    if (strcmp(RI_P, "P") != 0 || strcmp(RI_FRAMEBUFFER, "framebuffer") != 0) {
        fprintf(stderr, "FAIL: token values differ from today's\n");
        failures++;
    }
    if (RiBezierBasis[0][0] != -1.0f) {
        fprintf(stderr, "FAIL: RiBezierBasis is not the standard Bezier basis\n");
        failures++;
    }
    if (RiGaussianFilter(0.0f, 0.0f, 2.0f, 2.0f) <= 0.0f) {
        fprintf(stderr, "FAIL: RiGaussianFilter is not reachable\n");
        failures++;
    }
    if (failures)
        return 1;
    printf("ri.h and common/riVocabulary.h declare the same vocabulary\n");
    return 0;
}
