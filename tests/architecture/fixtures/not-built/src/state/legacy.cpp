// Excluded from the fixture build (EXCLUDE state/legacy.cpp).
#include "shading/shader.h"

int traceRay(int depth);

int legacySlots() {
    return kShaderSlots + traceRay(1);
}
