/**
 * tests/shading_system/test_third_backend.cpp
 *
 * ShadingSystem_ThirdBackend (spec 033, SC-003, FR-002). A shader format the library knows nothing about, registered by
 * the test alone ("tst", test_backend.h), is found, loaded once, cached, bound (the key taken once, initialise() once
 * inside the arena) and executed through CShadingContext::execute(CShaderInstance *, float **), the neutral entry that
 * reaches the instance's own backend. No file outside src/libshader/ and this test was edited for it.
 * Red on the starting tree: shadingSystem.h and shaderBackend.h do not exist (a build failure).
 */
#include "test_backend.h"

using namespace sstest;

int main() {
    CContractServices svc;
    {
        CHarness h(svc, 4);
        CTstBackend backend("tst");
        registerShaderBackend(&backend, 5);
        tstLog().reset();
        {
            ShadingSystem system; // takes the registry's entries, "tst" among them
            CMapResolver resolver;
            resolver.add("tstshader", "tst", writeTstShader("tstshader", "surface", 0.25f, 0.5f, 0.75f));
            CRecordingHost host;
            const ShaderRequest request{"tstshader", SL_SURFACE, "tst", nullptr, &resolver};

            CShader *first = system.find(request);
            CT_EXPECT(first != nullptr);
            CT_EXPECT(tstLog().loads == 1);
            CT_EXPECT(first != nullptr && first->backend == &backend);
            CT_EXPECT(first != nullptr && first->type == SL_SURFACE);
            // cached: a second request does not call load() and answers the same description
            CT_EXPECT(system.find(request) == first);
            CT_EXPECT(tstLog().loads == 1);

            const CMemPage *before = globalArena.top;
            const char *memoryBefore = globalArena.top->memory;
            CShaderInstance *instance = system.bind(request, h.attributes, h.xform, host);
            CT_EXPECT(instance != nullptr);
            CT_EXPECT(host.keysTaken == 1); // taken once, after the load and the type check
            CT_EXPECT(tstLog().instantiates == 1);
            CT_EXPECT(tstLog().initialises == 1);
            CT_EXPECT(tstLog().loads == 1); // bind() found the cached description
            // initialise() ran between arenaBegin and arenaEnd: its allocation moved the page, and the end put it back
            CT_EXPECT(tstLog().arenaDuring != memoryBefore);
            CT_EXPECT(globalArena.top == before && globalArena.top->memory == memoryBefore);
            CT_EXPECT(host.lights.empty()); // a surface registers no light

            if (instance != nullptr) {
                // Executed through the neutral entry: the instance is not an RSL instance
                h.setupState();
                CShadingState *state = h.state();
                state->currentShaderInstance = nullptr; // an RSL-only member; the dispatcher goes through the description
                CMemPage *memory = nullptr;
                memoryInit(memory);
                float **locals = instance->prepare(memory, state->varying, state->numVertices);
                h.context->execute(instance, locals);
                memoryTini(memory);
                CT_EXPECT(tstLog().executes == 1);
                for (int i = 0; i < h.numPoints; i++) {
                    CT_EXPECT_NEAR(state->varying[VARIABLE_CI][3 * i + 0], 0.25f);
                    CT_EXPECT_NEAR(state->varying[VARIABLE_CI][3 * i + 1], 0.5f);
                    CT_EXPECT_NEAR(state->varying[VARIABLE_CI][3 * i + 2], 0.75f);
                }
                instance->detach();
            }
        }
        unregisterShaderBackend("tst");
    }
    return contract_test::finish();
}
