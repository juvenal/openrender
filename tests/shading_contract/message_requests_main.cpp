/**
 * tests/shading_contract/message_requests_main.cpp
 *
 * The entry point of test_shading_contract_message_requests (spec 033, Phase 1): argv[1] is "rslo" or "slo" (the latter
 * only with the JIT), argv[2] the family of builtins: matrices, attributes or messages (each in a process of its own, like the
 * RealAnswers cases of spec 026). One RiBegin session per process (GitHub #2); check_recorded.cmake runs it once per backend.
 */
#include <cstdio>
#include <cstring>
#include <string>

int caseMessageRequests(bool jit, const std::string &family);

int main(int argc, char **argv) {
    // The record is read from a pipe: line buffering keeps everything printed before a crash
    setvbuf(stdout, nullptr, _IOLBF, 0);
    const char *which = argc > 1 ? argv[1] : "";
    const std::string family = argc > 2 ? argv[2] : "";
    if (family == "matrices" || family == "attributes" || family == "messages") {
        if (strcmp(which, "rslo") == 0)
            return caseMessageRequests(false, family);
#ifdef SHADING_CONTRACT_HAVE_JIT
        if (strcmp(which, "slo") == 0)
            return caseMessageRequests(true, family);
#endif
    }
    fprintf(stderr, "usage: %s rslo|slo matrices|attributes|messages\n", argv[0]);
    return 2;
}
