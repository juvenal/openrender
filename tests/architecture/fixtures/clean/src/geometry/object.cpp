#include "geometry/object.h"
#include "common/os.h"

int geometryBound(const Attributes &a) {
    return a.sides * kAlign;
}
