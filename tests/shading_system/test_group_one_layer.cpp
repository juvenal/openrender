/**
 * tests/shading_system/test_group_one_layer.cpp
 *
 * ShadingSystem_GroupOneLayer (spec 033, US4, FR-011, FR-012, P-Q5, contracts/shader-group.md). A group of one layer is its
 * layer's instance:
 *   - groupPrepare()/groupExecute() of the one-layer group give Ci and Oi bit for bit equal to running the instance directly;
 *   - connect(), and a second shader() call, return false with CODE_INCAPABLE "Shader group connections and multi-layer
 *     groups are not supported before spec 034\n", once per call; the refused layer is neither loaded nor instantiated and
 *     the group keeps its first layer;
 *   - end() returns that instance with one reference.
 * The shader is cm_producer_surface (tests/shading_contract), whose Ci is color(cmfloat, 0, 0) with cmfloat 2.5.
 * Red on the starting tree: shaderGroup.h and shadingSystem.h do not exist (a build failure).
 */
#include "libshader/shading/shaderGroup.h"
#include "test_backend.h"

using namespace sstest;

namespace {

    // Run `run` on the harness' points and return Ci and Oi, 3 floats per point each
    template <typename F>
    SHarnessResult runPoints(CHarness &h, CShaderInstance *instance, F run) {
        h.setupState();
        CShadingState *ss = h.state();
        ss->currentShaderInstance = static_cast<CProgrammableShaderInstance *>(instance);
        CMemPage *memory = nullptr;
        memoryInit(memory);
        run(memory, ss);
        SHarnessResult r;
        for (int i = 0; i < h.numPoints; i++) {
            r.ci.insert(r.ci.end(), ss->varying[VARIABLE_CI] + 3 * i, ss->varying[VARIABLE_CI] + 3 * i + 3);
            r.oi.insert(r.oi.end(), ss->varying[VARIABLE_OI] + 3 * i, ss->varying[VARIABLE_OI] + 3 * i + 3);
        }
        ss->currentShaderInstance = nullptr;
        memoryTini(memory);
        return r;
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
        resolver.add("cm_producer_surface", "rslo", contract_test::shaderDir() + "/cm_producer_surface.rslo");
        resolver.add("tstlayer2", "tst", writeTstShader("tstlayer2", "surface", 0, 1, 0));
        CRecordingHost host;
        const ShaderRequest request{"cm_producer_surface", SL_SURFACE, "rslo", nullptr, &resolver};

        // the group of one layer
        ShaderGroupBuilder group(system);
        group.begin();
        CT_EXPECT(group.shader(request, h.attributes, h.xform, host, 0, nullptr, nullptr));

        // connect() is refused, once per call
        CT_EXPECT(!group.connect("a", "x", "b", "y"));
        CT_EXPECT(!group.connect("a", "x", "b", "y"));
        const char *refusal = "Shader group connections and multi-layer groups are not supported before spec 034\n";
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_ERROR, refusal) == 2);

        // a second layer is refused and neither loaded nor instantiated
        const ShaderRequest second{"tstlayer2", SL_SURFACE, "tst", nullptr, &resolver};
        CT_EXPECT(!group.shader(second, h.attributes, h.xform, host, 0, nullptr, nullptr));
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_ERROR, refusal) == 3);
        CT_EXPECT(tstLog().loads == 0 && tstLog().instantiates == 0);
        CT_EXPECT(resolver.asked("tstlayer2.tst") == 0);
        CT_EXPECT(host.keysTaken == 1); // only the first layer took a key

        // the group is its first layer's instance, with one reference
        CShaderInstance *grouped = group.end();
        CT_EXPECT(grouped != nullptr);
        if (grouped == nullptr) {
            installDiagnosticSink(previous);
            return contract_test::finish();
        }
        CT_EXPECT(grouped->refCount == 1);
        CT_EXPECT(strcmp(grouped->getName(), "cm_producer_surface") == 0);

        // the same shader bound directly (the cached description), run directly
        CShaderInstance *direct = system.bind(request, h.attributes, h.xform, host);
        CT_EXPECT(direct != nullptr);
        if (direct != nullptr) {
            const SHarnessResult byGroup = runPoints(h, grouped, [&](CMemPage *&memory, CShadingState *ss) {
                float **locals = libshader::groupPrepare(grouped, memory, ss->varying, ss->numVertices);
                libshader::groupExecute(*h.context, grouped, locals);
            });
            const SHarnessResult byInstance = runPoints(h, direct, [&](CMemPage *&memory, CShadingState *ss) {
                float **locals = direct->prepare(memory, ss->varying, ss->numVertices);
                h.context->execute(direct, locals);
            });
            CT_EXPECT(byGroup.ci.size() == 3u * h.numPoints && byInstance.ci.size() == byGroup.ci.size());
            CT_EXPECT(byGroup.ci.size() == byInstance.ci.size() &&
                      memcmp(byGroup.ci.data(), byInstance.ci.data(), byGroup.ci.size() * sizeof(float)) == 0);
            CT_EXPECT(byGroup.oi.size() == byInstance.oi.size() &&
                      memcmp(byGroup.oi.data(), byInstance.oi.data(), byGroup.oi.size() * sizeof(float)) == 0);
            for (int i = 0; i < h.numPoints; i++)
                CT_EXPECT_NEAR(byGroup.ciAt(i, 0), 2.5f); // and it is the shader's own answer
            direct->detach();
        }
        grouped->detach();
    }
    unregisterShaderBackend("tst");
    installDiagnosticSink(previous);
    return contract_test::finish();
}
