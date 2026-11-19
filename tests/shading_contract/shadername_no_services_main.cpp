/**
 * tests/shading_contract/shadername_no_services_main.cpp
 *
 * The entry point of test_shading_contract_shadername_no_services (spec 033, Phase 2): argv[1] is Interp or Jit (the latter only
 * with the JIT). The "once per kind" warning is process-wide, so each backend runs in a process of its own.
 */
#include <cstdio>
#include <cstring>

int caseShadernameNoServices(bool jit);

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "Interp") == 0)
        return caseShadernameNoServices(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "Jit") == 0)
        return caseShadernameNoServices(true);
#endif
    fprintf(stderr, "usage: %s Interp|Jit\n", argv[0]);
    return 2;
}
