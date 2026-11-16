/**
 * tests/shading_contract/bind_spaces_main.cpp
 *
 * The entry point of test_shading_contract_bind_spaces (spec 033, Phase 1): argv[1] is "rslo" or "slo" (the latter only
 * with the JIT). One RiBegin session per process (GitHub #2); check_recorded.cmake runs it once per backend.
 */
#include <cstdio>
#include <cstring>

int caseBindSpaces(bool jit);

int main(int argc, char **argv) {
    // The record is read from a pipe: line buffering keeps everything printed before a crash
    setvbuf(stdout, nullptr, _IOLBF, 0);
    const char *which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "rslo") == 0)
        return caseBindSpaces(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "slo") == 0)
        return caseBindSpaces(true);
#endif
    fprintf(stderr, "usage: %s rslo|slo\n", argv[0]);
    return 2;
}
