/**
 * tests/shading_contract/standalone_main.cpp
 *
 * The entry point of test_shading_contract_standalone (spec 026, T044): the program that runs the
 * shading library against the services double with no renderer behind it. argv[1] names the case; each
 * CTest entry runs one case in a process of its own (the "once per kind" warnings are process-wide).
 * The *Jit cases exist only when the build has the LLVM JIT (SHADING_CONTRACT_HAVE_JIT).
 */
#include <cstdio>
#include <cstring>

#include "cases.h"

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";

    if (strcmp(which, "StandaloneLink") == 0)
        return caseStandaloneLink();
    if (strcmp(which, "AttributesInterp") == 0)
        return caseAttributes(false);
    if (strcmp(which, "MessagesInterp") == 0)
        return caseMessages(false);
    if (strcmp(which, "MatricesInterp") == 0)
        return caseMatrices(false);
    if (strcmp(which, "TraceRequestsInterp") == 0)
        return caseTraceRequests(false);
    if (strcmp(which, "TypeMismatch_AttributeInterp") == 0)
        return caseTypeMismatchAttribute(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "AttributesJit") == 0)
        return caseAttributes(true);
    if (strcmp(which, "MessagesJit") == 0)
        return caseMessages(true);
    if (strcmp(which, "MatricesJit") == 0)
        return caseMatrices(true);
    if (strcmp(which, "TraceRequestsJit") == 0)
        return caseTraceRequests(true);
    if (strcmp(which, "TypeMismatch_AttributeJit") == 0)
        return caseTypeMismatchAttribute(true);
#endif
    if (strcmp(which, "NoServices_Matrix") == 0)
        return caseNoServicesMatrix();
    if (strcmp(which, "NoServices_Attribute") == 0)
        return caseNoServicesAttribute();
    if (strcmp(which, "NoServices_Message") == 0)
        return caseNoServicesMessage();
    if (strcmp(which, "NoServices_Tracer") == 0)
        return caseNoServicesTracer();
    if (strcmp(which, "ShaderGlobalsDouble") == 0)
        return caseShaderGlobalsDouble();

    fprintf(stderr,
            "usage: %s StandaloneLink|AttributesInterp|MessagesInterp|MatricesInterp|TraceRequestsInterp|"
            "AttributesJit|MessagesJit|MatricesJit|TraceRequestsJit|TypeMismatch_Attribute{Interp,Jit}|NoServices_Matrix|NoServices_Attribute|"
            "NoServices_Message|NoServices_Tracer|ShaderGlobalsDouble\n",
            argv[0]);
    return 2;
}
