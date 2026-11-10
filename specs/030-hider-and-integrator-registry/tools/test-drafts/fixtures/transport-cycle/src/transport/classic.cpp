#include "hiders/hider.h"

// Transport must not depend on Hiders: this include is the violation, and with the allowed edge below it is a cycle.
int classicSlots() {
    return kHiderSlots;
}
