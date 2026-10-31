/**
 * tests/shading_contract/cases.h
 *
 * The contract cases of the standalone program (test_shading_contract_standalone), one function per
 * file. standalone_main.cpp dispatches on argv[1]; each CTest entry runs one case in its own process.
 */
#ifndef TESTS_SHADING_CONTRACT_CASES_H
#define TESTS_SHADING_CONTRACT_CASES_H

int caseStandaloneLink();                       // case_link.cpp                 US1 AS1, SC-001
int caseAttributes(bool jit);                   // case_attributes.cpp           US3 AS1, FR-005, FR-007, SC-003
int caseMessages(bool jit);                     // case_messages.cpp             US3 AS1, FR-006, FR-007, SC-003
int caseMatrices(bool jit);                     // case_matrices.cpp             US4 AS1-AS3, FR-008, FR-009, SC-004
int caseTraceRequests(bool jit);                // case_trace_requests.cpp       US6 AS1, FR-010, FR-011
int caseNoServicesMatrix();                     // case_no_services.cpp          US1 AS3, FR-012, FR-023(f)
int caseNoServicesAttribute();                  // case_no_services.cpp
int caseNoServicesMessage();                    // case_no_services.cpp
int caseNoServicesTracer();                     // case_no_services.cpp
int caseShaderGlobalsDouble();                  // case_globals_double.cpp       US5 AS1, AS3, FR-015, FR-016

#endif
