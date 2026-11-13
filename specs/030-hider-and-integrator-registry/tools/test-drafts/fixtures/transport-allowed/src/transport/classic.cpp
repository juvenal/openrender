#include "shading/shader.h"
#include "transport/capability.h"

// A Transport file may include Shading (spec 030, the amended Principle VIII table).
int classicSlots() {
    return kShaderSlots + kCapabilityCount;
}
