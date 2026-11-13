#include "hiders/hider.h"
#include "transport/capability.h"

int hiderCapabilities() {
    return kCapabilityCount + kHiderSlots;
}
