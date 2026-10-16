/**
 * tests/shading_parity/test_service_lookup.cpp
 *
 * The shading library's service-lookup helpers (spec
 * 024-shading-services-routing, T025, FR-001 to FR-003;
 * contracts/renderer-services.md).
 *
 * The shading library no longer calls CRenderer for DSO shadeops, filters,
 * global IDs and variable lookups: five helpers in namespace libshader
 * (serviceLookup.h) forward to the default renderer services. With a stub
 * installed, each must return the stub's sentinel, which proves the lookup
 * goes through the interface and nowhere else.
 *
 * No renderer session is started: the stub is installed directly.
 */

#include <cstdio>

#include "libshader/shading/dsoHandle.h"
#include "libshader/shading/serviceLookup.h"
#include "libshader/shading/shading.h"
#include "recording_services.h"
#include "ri/parse/ri.h"

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (expr) {                                                            \
            ++g_passed;                                                        \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

int main() {
    CStubServices stub;
    CShadingContext::setDefaultServices(&stub);

    EXPECT_TRUE(libshader::lookupDSO("anyshadeop", "f=f") == CStubServices::dso());
    EXPECT_TRUE(libshader::lookupFilter("anyfilter") == RiBoxFilter);
    EXPECT_TRUE(libshader::lookupStepFilter("anystepfilter") == RiBoxStepFilter);
    EXPECT_TRUE(libshader::lookupGlobalID("anycategory") == CStubServices::kGlobalID);
    EXPECT_TRUE(libshader::lookupVariable("anyvariable") == CStubServices::variable());

    CShadingContext::setDefaultServices(nullptr);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
