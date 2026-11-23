/**
 * tests/shading_system/test_light_category.cpp
 *
 * ShadingSystem_LightCategoryFromRib (GitHub #199). A light's "__category" given in the RIB ("string __category" on the LightSource
 * statement) is the category the light answers to; the shader's declared default only counts when the RIB gives none. The RIB's
 * parameters reach the instance after ShadingSystem::bind() (ShaderGroupBuilder::shader() calls setParameters()), so the categories the
 * active-light loop reads (CShaderInstance::categories) must be built from the instance's parameters after that call.
 *
 *   a light with a default category and a RIB category      the RIB's
 *   a light with an empty default and a RIB category        the RIB's (the shader had no category at all)
 *   a light with a default category and no RIB category     the default
 *   a surface given a "__category"                          no categories (only lights have them)
 *   the light is registered once, with the same instance that carries the categories
 */
#include "libshader/shading/shaderGroup.h"
#include "test_backend.h"

using namespace sstest;

namespace {

    CShaderInstance *bindThroughGroup(ShadingSystem &system, const ShaderRequest &request, CHarness &h, CRecordingHost &host, const char *category) {
        ShaderGroupBuilder group(system);
        group.begin();
        const char *tokens[1] = {"__category"};
        const char *value = category;
        const void *values[1] = {&value};
        group.shader(request, h.attributes, h.xform, host, category != nullptr ? 1 : 0, tokens, values);
        return group.end();
    }

    // the IDs of an instance's categories, in order (the list ends with 0)
    std::vector<int> idsOf(const CShaderInstance *instance) {
        std::vector<int> ids;
        if (instance != nullptr && instance->categories != nullptr)
            for (const int *c = instance->categories; *c != 0; c++)
                ids.push_back(*c);
        return ids;
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
        resolver.add("tstlight", "tst", writeTstShader("tstlight", "light", 1, 1, 1, "defaultcat"));
        resolver.add("tstbarelight", "tst", writeTstShader("tstbarelight", "light", 1, 1, 1)); // declares no default category
        resolver.add("tstsurface", "tst", writeTstShader("tstsurface", "surface", 1, 0, 0));
        CRecordingHost host;
        host.events = &tstLog().events;

        const int defaultId = svc.getGlobalID("defaultcat");
        const int otherId = svc.getGlobalID("othercat");
        CT_EXPECT(defaultId != 0 && otherId != 0 && defaultId != otherId);

        {
            const ShaderRequest request{"tstlight", SL_LIGHTSOURCE, "tst", nullptr, &resolver};
            CShaderInstance *instance = bindThroughGroup(system, request, h, host, "othercat");
            CT_EXPECT(instance != nullptr);
            CT_EXPECT(idsOf(instance) == std::vector<int>({otherId}));
            CT_EXPECT(host.lights.size() == 1 && host.lights[0] == instance);
            if (instance != nullptr)
                instance->detach();
        }
        {
            const ShaderRequest request{"tstbarelight", SL_LIGHTSOURCE, "tst", nullptr, &resolver};
            CShaderInstance *instance = bindThroughGroup(system, request, h, host, "othercat");
            CT_EXPECT(instance != nullptr);
            CT_EXPECT(idsOf(instance) == std::vector<int>({otherId}));
            if (instance != nullptr)
                instance->detach();
        }
        {
            const ShaderRequest request{"tstlight", SL_LIGHTSOURCE, "tst", nullptr, &resolver};
            CShaderInstance *instance = bindThroughGroup(system, request, h, host, nullptr);
            CT_EXPECT(instance != nullptr);
            CT_EXPECT(idsOf(instance) == std::vector<int>({defaultId}));
            if (instance != nullptr)
                instance->detach();
        }
        {
            const ShaderRequest request{"tstsurface", SL_SURFACE, "tst", nullptr, &resolver};
            CShaderInstance *instance = bindThroughGroup(system, request, h, host, "othercat");
            CT_EXPECT(instance != nullptr && instance->categories == nullptr);
            if (instance != nullptr)
                instance->detach();
        }
    }
    unregisterShaderBackend("tst");
    installDiagnosticSink(previous);
    return contract_test::finish();
}
