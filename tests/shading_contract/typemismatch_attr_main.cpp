#include <cstdio>
#include <cstring>

#include "typemismatch_cases.h"

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";

    if (strcmp(which, "TypeMismatchReal_AttributeInterp") == 0)
        return caseTypeMismatchRealAttribute(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "TypeMismatchReal_AttributeJit") == 0)
        return caseTypeMismatchRealAttribute(true);
#endif
    fprintf(stderr, "usage: %s TypeMismatchReal_Attribute{Interp,Jit}\n", argv[0]);
    return 2;
}
