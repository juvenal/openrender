/**
 * tests/shading_contract/message_requests_both_main.cpp
 *
 * The entry point of test_shading_contract_message_requests_both (spec 033, Phase 2): argv[1] is "rslo" or "slo" (the latter
 * only with the JIT), argv[2] the family: matrices, attributes or messages. One RiBegin session per process (GitHub #2, #176);
 * check_all_modes.cmake runs it once per backend and family.
 */
#include <cstdio>
#include <cstring>
#include <string>

int caseMessageRequestsBoth(bool jit, const std::string &family);

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    const char *which = argc > 1 ? argv[1] : "";
    const std::string family = argc > 2 ? argv[2] : "";
    if (family == "matrices" || family == "attributes" || family == "messages") {
        if (strcmp(which, "rslo") == 0)
            return caseMessageRequestsBoth(false, family);
#ifdef SHADING_CONTRACT_HAVE_JIT
        if (strcmp(which, "slo") == 0)
            return caseMessageRequestsBoth(true, family);
#endif
    }
    fprintf(stderr, "usage: %s rslo|slo matrices|attributes|messages\n", argv[0]);
    return 2;
}
