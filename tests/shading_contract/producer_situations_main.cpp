/**
 * tests/shading_contract/producer_situations_main.cpp
 *
 * The entry point of test_shading_contract_producer_situations (spec 029): with no argument it runs every scene (each in a
 * child process); the hidden "scene <name>" form renders one, as ShadingContract_Situations does.
 */
#include "cases_029.h"

int main(int argc, char **argv) {
    return caseProducerSituations(argc, argv);
}
