/**
 * tests/shading_system/test_bind_unknown_space.cpp
 *
 * ShadingSystem_BindUnknownSpace (spec 033, P-Q2, FR-009, GitHub #134, research R6). A parameter default written in a
 * coordinate system the services do not know (point "nosuch" (1,2,3)), bound by the interpreter's init or by the JIT's:
 * the bind-time lookup asks getMatrix with phase == EMatrixPhase::Bind, is answered NotFound, and the default is the point
 * itself (the identity), with no diagnostic at all (the JIT's present answer; the interpreter's is undefined today). One
 * process per backend: `rslo` or `slo` (the latter only with the JIT).
 *
 * It uses what exists after Phase 3 only (the loaders, the harness, libshader::initialiseInstance(), the services double),
 * not the ShadingSystem facade of Phase 4. Red on the starting tree: EMatrixPhase, MatrixRequest::phase and
 * libshader::initialiseInstance() do not exist (a build failure).
 */
#include <cstring>

#include "contract_forwarder.h"
#include "contract_harness.h"
#include "case_common.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/serviceTypes.h"

using namespace openrender::shading;

namespace {

    // The services double with one change: a Bind-phase request is answered NotFound (and counted)
    class CBindProbe : public CContractForwarder {
        public:
            using CContractForwarder::CContractForwarder;
            mutable int bindRequests = 0, bindRequestsForNosuch = 0;
            MatrixAnswer getMatrix(const MatrixRequest &r) const override {
                if (r.phase == EMatrixPhase::Bind) {
                    ++bindRequests;
                    if (strcmp(r.name, "nosuch") == 0)
                        ++bindRequestsForNosuch;
                    MatrixAnswer a;
                    a.status = EQueryStatus::NotFound;
                    return a;
                }
                return CContractForwarder::getMatrix(r);
            }
    };

} // namespace

int main(int argc, char **argv) {
    const bool jit = argc > 1 && strcmp(argv[1], "slo") == 0;
#ifndef OPENRENDER_HAVE_LLVM
    if (jit) {
        fprintf(stderr, "no JIT in this build\n");
        return 77;
    }
#endif
    CContractServices svc;
    CBindProbe probe(&svc);
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        CShadingContext::setDefaultServices(&probe); // the lookups of the bind-time init go through the probe
        CProgrammableShaderInstance *instance = h.load("cm_nosuch", jit);
        CT_EXPECT(instance != nullptr);
        if (instance != nullptr) {
            arenaBegin(globalArena);
            libshader::initialiseInstance(*instance);
            arenaEnd(globalArena);

            float p[3] = {-1, -1, -1};
            CT_EXPECT(instance->getParameter("p", p, nullptr, nullptr) == TRUE);
            CT_EXPECT(p[0] == 1.0f && p[1] == 2.0f && p[2] == 3.0f); // the identity: the point is unchanged
        }
        CT_EXPECT(probe.bindRequestsForNosuch >= 1); // the question went through the services, as a Bind request
        CShadingContext::setDefaultServices(&svc);
    }
    int diagnostics = 0; // warnings and errors ("Unknown coordinate system" is a run-time warning; bind time gives nothing)
    for (const CRecordingSink::SReport &r : sink.reports)
        diagnostics += (r.severity >= DIAGNOSTIC_WARNING);
    CT_EXPECT(diagnostics == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
