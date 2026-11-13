/**
 * tests/shading_contract/cases_056.h
 *
 * The contract cases spec 056 adds (tasks.md, Phase 1). Each lives in an executable of its own (adding a case to an
 * existing executable would put the approved cases already in it at risk).
 */
#ifndef TESTS_SHADING_CONTRACT_CASES_056_H
#define TESTS_SHADING_CONTRACT_CASES_056_H

int caseVolumeSegments(bool jit); // case_volume_segments.cpp  T013-T016  US1 to US4, FR-001 to FR-010, FR-014 (rendered program)
int caseMessageSide(bool jit);    // case_message_side.cpp     T017       US7, FR-013 (standalone program)

#endif
