// Spec 023 (research R8, contracts/variable-lookup.md §1): the Graphics State
// variable-lookup hook that lets pl.cpp resolve declared variable names
// without calling the renderer core.
//
//   - with nothing installed, lookupVariable() returns nullptr
//   - an installed lookup answers, and installVariableLookup(nullptr) clears it
//   - RiBegin installs the renderer core's lookup (CRenderer::retrieveVariable)
//     and RiEnd removes it again

#include <cstdio>
#include <cstring>

#include "common/variable.h"
#include "ri.h"
#include "ri/state/variableLookup.h"

namespace {

int failures = 0;

#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
            failures++;                                          \
        }                                                        \
    } while (0)

CVariable fooVariable;

CVariable *fakeLookup(const char *name) {
    return strcmp(name, "foo") == 0 ? &fooVariable : nullptr;
}

} // namespace

int main() {
    CHECK(lookupVariable("P") == nullptr, "with nothing installed, a lookup returns nullptr");

    installVariableLookup(&fakeLookup);
    CHECK(lookupVariable("foo") == &fooVariable, "the installed lookup answers for \"foo\"");
    CHECK(lookupVariable("bar") == nullptr, "the installed lookup returns nullptr for \"bar\"");

    installVariableLookup(nullptr);
    CHECK(lookupVariable("foo") == nullptr, "installVariableLookup(nullptr) clears the lookup");

    RiBegin(RI_NULL);
    CVariable *p = lookupVariable("P");
    CHECK(p != nullptr, "inside RiBegin, the renderer core's lookup resolves \"P\"");
    if (p != nullptr)
        CHECK(strcmp(p->name, "P") == 0, "the lookup for \"P\" returns the variable P, got \"%s\"", p->name);
    RiEnd();
    CHECK(lookupVariable("P") == nullptr, "after RiEnd, no lookup is installed");

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all variable lookup checks passed\n");
    return 0;
}
