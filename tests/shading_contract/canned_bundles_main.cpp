/**
 * tests/shading_contract/canned_bundles_main.cpp
 *
 * The entry point of test_shading_contract_canned_bundles (spec 029): caseCannedBundles() under one backend per process.
 * argv[1] is Interp, or Jit when the build has the LLVM JIT (SHADING_CONTRACT_HAVE_JIT).
 */
#include <cstdio>
#include <cstring>

#include "cases_029.h"

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "Interp") == 0)
        return caseCannedBundles(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "Jit") == 0)
        return caseCannedBundles(true);
#endif
    fprintf(stderr, "usage: %s Interp|Jit\n", argv[0]);
    return 2;
}
