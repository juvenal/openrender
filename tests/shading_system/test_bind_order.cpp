/**
 * tests/shading_system/test_bind_order.cpp
 *
 * ShadingSystem_BindOrder (spec 033, FR-008, FR-029, contracts/shading-system.md "Bind" and
 * contracts/light-shader-entry-points.md "The bind order"). The order of the binding of one shader layer, observed through
 * the "tst" backend, the bind host double and ShaderGroupBuilder (which calls bind() and then setParameters()):
 *
 *   missing shader     CODE_NOSHADER "Failed to find shader \"%s\"\n", no instance, no key taken
 *   wrong type         CODE_NOSHADER "Shader \"%s\" is not of the expected type\n", no instance, no key taken
 *   a surface          load, instantiate, takeInstanceKey, initialise, setParameters
 *   a light            load, instantiate, takeInstanceKey, createCategories (reads the DEFAULT __category: the RIB's parameters are
 *                      not set yet), registerLight, initialise, setParameters
 *   initialise()       once per instance, inside arenaBegin/arenaEnd(globalArena)
 *
 * Red on the starting tree: shadingSystem.h, shaderGroup.h and shaderBackend.h do not exist (a build failure).
 */
#include "libshader/shading/shaderGroup.h"
#include "test_backend.h"

using namespace sstest;

namespace {

    bool equal(const std::vector<std::string> &a, const std::vector<std::string> &b) { return a == b; }

    CShaderInstance *bindThroughGroup(ShadingSystem &system, const ShaderRequest &request, CHarness &h, CRecordingHost &host, const char *category) {
        ShaderGroupBuilder group(system);
        group.begin();
        const char *tokens[1] = {"__category"};
        const char *value = category;
        const void *values[1] = {&value};
        const bool bound = group.shader(request, h.attributes, h.xform, host, category != nullptr ? 1 : 0, tokens, values);
        CShaderInstance *instance = group.end();
        CT_EXPECT(bound == (instance != nullptr));
        return instance;
    }

} // namespace

int main() {
    CContractServices svc;
    CHarness h(svc, 4);
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    CTstBackend backend("tst");
    registerShaderBackend(&backend, 5);
    tstLog().reset();
    {
        ShadingSystem system;
        CMapResolver resolver;
        resolver.add("tstsurface", "tst", writeTstShader("tstsurface", "surface", 1, 0, 0));
        resolver.add("tstlight", "tst", writeTstShader("tstlight", "light", 1, 1, 1, "defaultcat"));
        resolver.add("tstwrongtype", "tst", writeTstShader("tstwrongtype", "light", 1, 1, 1)); // a light, asked for as a surface
        CRecordingHost host;
        std::vector<std::string> &events = tstLog().events;
        host.events = &events;

        // a missing shader: the diagnostic, no instance, no key
        {
            const ShaderRequest request{"nosuch", SL_SURFACE, "tst", nullptr, &resolver};
            CT_EXPECT(system.bind(request, h.attributes, h.xform, host) == nullptr);
            CT_EXPECT(sink.count(CODE_NOSHADER, DIAGNOSTIC_ERROR, "Failed to find shader \"nosuch\"\n") == 1);
            CT_EXPECT(host.keysTaken == 0 && tstLog().instantiates == 0);
        }
        // the wrong type: the diagnostic, no instance, no key
        {
            const ShaderRequest request{"tstwrongtype", SL_SURFACE, "tst", nullptr, &resolver};
            CT_EXPECT(system.bind(request, h.attributes, h.xform, host) == nullptr);
            CT_EXPECT(sink.count(CODE_NOSHADER, DIAGNOSTIC_ERROR, "Shader \"tstwrongtype\" is not of the expected type\n") == 1);
            CT_EXPECT(host.keysTaken == 0 && tstLog().instantiates == 0 && tstLog().initialises == 0);
        }

        // a surface
        events.clear();
        {
            const ShaderRequest request{"tstsurface", SL_SURFACE, "tst", nullptr, &resolver};
            const char *memoryBefore = globalArena.top->memory;
            CShaderInstance *instance = bindThroughGroup(system, request, h, host, "other");
            CT_EXPECT(instance != nullptr);
            CT_EXPECT(equal(events, {"load", "instantiate", "takeInstanceKey", "initialise", "setParameters"}));
            CT_EXPECT(tstLog().arenaDuring != memoryBefore && globalArena.top->memory == memoryBefore); // initialise() ran inside the arena region
            CT_EXPECT(host.lights.empty());
            CT_EXPECT(instance != nullptr && instance->instanceKey == 1);
            if (instance != nullptr)
                instance->detach();
        }

        // a light: categories from the default __category, then registration, then init, then the RIB's parameters
        events.clear();
        {
            const ShaderRequest request{"tstlight", SL_LIGHTSOURCE, "tst", nullptr, &resolver};
            CShaderInstance *instance = bindThroughGroup(system, request, h, host, "othercat");
            CT_EXPECT(instance != nullptr);
            CT_EXPECT(equal(events, {"load", "instantiate", "takeInstanceKey", "getParameter(__category)", "registerLight", "initialise", "setParameters"}));
            CT_EXPECT(host.lights.size() == 1 && host.lights[0] == instance);
            CT_EXPECT(instance != nullptr && instance->instanceKey == 2);
            if (instance != nullptr && instance->categories != nullptr) {
                // the ID of "defaultcat", not of "othercat": the parameters were set after the categories were read
                CT_EXPECT(instance->categories[0] == svc.getGlobalID("defaultcat"));
                CT_EXPECT(instance->categories[0] != svc.getGlobalID("othercat"));
            }
            else {
                CT_EXPECT(false); // a light with a __category has categories
            }
            if (instance != nullptr)
                instance->detach();
        }
        CT_EXPECT(tstLog().initialises == 2);
    }
    unregisterShaderBackend("tst");
    installDiagnosticSink(previous);
    return contract_test::finish();
}
