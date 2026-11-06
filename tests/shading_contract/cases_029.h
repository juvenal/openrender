/**
 * tests/shading_contract/cases_029.h
 *
 * The contract cases spec 029 adds (tasks.md, Phase 2). Each lives in an executable of its own (adding a case that
 * cannot compile to an existing executable would break the approved cases already in it).
 */
#ifndef TESTS_SHADING_CONTRACT_CASES_029_H
#define TESTS_SHADING_CONTRACT_CASES_029_H

int caseStandaloneContext(bool jit);          // case_standalone_context.cpp          T-S2  US1 AS3, US4 AS1, FR-007, SC-004
int casePhotonLookup(bool jit);               // case_photon_lookup.cpp               T-P1  FR-012, US5 AS3
int caseCannedBundles(bool jit);              // case_canned_bundles.cpp              T-B1  FR-011, US5 AS3
int caseIlluminationHookInterface(bool jit);  // case_illumination_hook_interface.cpp T-H1  FR-013, US5 AS4
int caseHiderTracer();                        // case_hider_tracer.cpp                T-T1  FR-016, US6 AS2, SC-007
int caseRayLevel();                           // case_ray_level.cpp                   T-T2  FR-017
int casePointSource(bool jit);                // case_point_source.cpp                T-G2  FR-018, FR-020, FR-021, US7 AS3
int caseProducerSituations(int argc, char **argv); // case_producer_situations.cpp    T-G1  FR-019, US7 AS1, SC-008

#endif
