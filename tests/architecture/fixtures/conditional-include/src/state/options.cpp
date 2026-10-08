// ARCHFIXTURE_WITH_SHADING is never defined for this fixture: the include is
// written in the source but inactive in the build, like an option-dependent
// include in a configuration with that option off.
#ifdef ARCHFIXTURE_WITH_SHADING
#include "shading/shader.h"
#endif

int optionCount() {
    return 1;
}
