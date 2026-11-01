/**
 * tests/shading_contract/rendered_main.cpp
 *
 * The entry point of test_shading_contract_rendered (spec 026, T044): the contract cases that need a
 * renderer behind them (real objects, real bound shaders, the Hiders' bundles and irradiance cache).
 * argv[1] names the case; each CTest entry runs one case in a process of its own (GitHub #2). The
 * *Jit cases exist only when the build has the LLVM JIT (SHADING_CONTRACT_HAVE_JIT).
 */
#include <cstdio>
#include <cstring>

#include "rendered_cases.h"

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";

    if (strcmp(which, "RealAnswersInterp_Matrices") == 0)
        return caseRealAnswers(false, "Matrices");
    if (strcmp(which, "RealAnswersInterp_Attributes") == 0)
        return caseRealAnswers(false, "Attributes");
    if (strcmp(which, "RealAnswersInterp_Messages") == 0)
        return caseRealAnswers(false, "Messages");
    if (strcmp(which, "GatherRequestsInterp") == 0)
        return caseGatherRequests(false);
    if (strcmp(which, "CannedTraceInterp") == 0)
        return caseCannedTrace(false);
    if (strcmp(which, "NullObject_AttributesInterp") == 0)
        return caseNullObjectAttributes(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "RealAnswersJit_Matrices") == 0)
        return caseRealAnswers(true, "Matrices");
    if (strcmp(which, "RealAnswersJit_Attributes") == 0)
        return caseRealAnswers(true, "Attributes");
    if (strcmp(which, "RealAnswersJit_Messages") == 0)
        return caseRealAnswers(true, "Messages");
    if (strcmp(which, "GatherRequestsJit") == 0)
        return caseGatherRequests(true);
    if (strcmp(which, "CannedTraceJit") == 0)
        return caseCannedTrace(true);
    if (strcmp(which, "NullObject_AttributesJit") == 0)
        return caseNullObjectAttributes(true);
#endif
    if (strcmp(which, "NoTracer") == 0)
        return caseNoTracer();
    if (strcmp(which, "DoubleGlobalsMatchRenderer") == 0)
        return caseDoubleGlobalsMatchRenderer();

    fprintf(stderr,
            "usage: %s RealAnswers{Interp,Jit}_{Matrices,Attributes,Messages}|GatherRequests{Interp,Jit}|"
            "CannedTrace{Interp,Jit}|NullObject_Attributes{Interp,Jit}|NoTracer|DoubleGlobalsMatchRenderer\n",
            argv[0]);
    return 2;
}
