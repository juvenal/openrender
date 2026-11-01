/**
 * tests/shading_contract/rendered_cases.h
 *
 * The contract cases of the rendered program (test_shading_contract_rendered), one function per file.
 * rendered_main.cpp dispatches on argv[1]; each CTest entry runs one case in a process of its own
 * (GitHub #2: one RiBegin/RiEnd session per process).
 */
#ifndef TESTS_SHADING_CONTRACT_RENDERED_CASES_H
#define TESTS_SHADING_CONTRACT_RENDERED_CASES_H

int caseRealAnswers(bool jit, const char *family); // case_real_answers.cpp    US3 AS2, US2 AS3, FR-022, SC-003
int caseGatherRequests(bool jit);                  // case_gather_requests.cpp  US6 AS1, FR-010, FR-011
int caseCannedTrace(bool jit);                     // case_canned_trace.cpp     US6 AS1, AS3
int caseNoTracer();                                // case_no_tracer.cpp        US6 AS3, FR-012
int caseNullObjectAttributes(bool jit);            // case_null_object.cpp      US3 AS5, FR-027, SC-014
int caseDoubleGlobalsMatchRenderer();              // case_double_globals.cpp   R10

#endif
