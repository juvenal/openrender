/**
 * tests/shading_contract/hider_tracer_main.cpp
 *
 * The entry point of test_shading_contract_hider_tracer (spec 029): caseHiderTracer(), no argument. It links
 * ribRender (it constructs real hiders), so it runs one RiBegin/RiEnd session per process (GitHub #2).
 */
#include "cases_029.h"

int main() {
    return caseHiderTracer();
}
