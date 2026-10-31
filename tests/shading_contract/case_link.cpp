/**
 * tests/shading_contract/case_link.cpp
 *
 * ShadingContract_StandaloneLink (spec 026, T051; US1 AS1, FR-004, FR-024, SC-001).
 *
 * The program that runs this case links libshader_shading, libRIBCommon, the State-only library and
 * the load stub, and nothing of the renderer. The case installs the services double, builds a shading
 * state from the double's 29 globals, runs a trivial shader under both backends and checks that
 *   - the double saw the state's global-variable queries (the state was built from it, not from a renderer);
 *   - the shader ran (Ci is what the shader writes);
 *   - nothing reached a renderer-only lookup: the double saw no photon map, cache, texture or DSO request.
 * If the program had needed a renderer to start, it would not have started: the macOS fixups bind every
 * import at load, and the load stub (load_stub.cpp) defines the six that remain.
 *
 * The link command itself is checked by ShadingContract_StandaloneLinkLine (check_link_line.cmake).
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"

int caseStandaloneLink() {
    CContractServices svc;
    {
        CHarness h(svc, 4);
        CT_EXPECT(h.state() != nullptr);
        CT_EXPECT(svc.globalQueries > 0);
        CT_EXPECT(svc.numGlobalVariables() == (int)VARIABLE_CONSTANTWIDTH + 1);

        for (int backend = 0; backend < 2; backend++) {
            const bool jit = (backend == 1);
#ifndef SHADING_CONTRACT_HAVE_JIT
            if (jit)
                continue;
#endif
            CProgrammableShaderInstance *surface = h.load("cm_producer_surface", jit);
            CT_EXPECT(surface != nullptr);
            if (surface == nullptr)
                continue;
            const SHarnessResult r = h.runSurface(surface);
            for (int i = 0; i < h.numPoints; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), 2.5f); // the producer writes Ci = color(cmfloat, 0, 0)
                CT_EXPECT_NEAR(r.oiAt(i, 0), 1.0f);
            }
        }
    }

    // Nothing from a renderer
    CT_EXPECT(svc.count("getPhotonMap") == 0);
    CT_EXPECT(svc.count("getCache") == 0);
    CT_EXPECT(svc.count("getTexture3d") == 0);
    CT_EXPECT(svc.count("textureSystem") == 0);
    CT_EXPECT(svc.count("getDSO") == 0);
    CT_EXPECT(svc.count("tracer") == 0);

    return contract_test::finish();
}
