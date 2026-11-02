/**
 * tests/shading_contract/case_offending_object.cpp
 *
 * ShadingContract_OffendingObject (spec 026, T124; FR-003, Q11, C6, B10).
 *
 * A shader error names the object that caused it: Shading tells the services through
 * setOffendingObject(ObjectHandle) (execute.cpp's scripterror, which reaches it through the context's
 * rendererSetOffendingObject(objectHandle()) helper). The handle replaces the CObject pointer, so execute.cpp no
 * longer needs CSurface's complete type (the DB-006 edge to surface.h). This test asserts the new signature:
 *   - with the CContractForwarder installed over the real services, the context's current object is a real
 *     CRProbeSurface; one rendererSetOffendingObject(objectHandle()) makes the forwarder record exactly one call
 *     with an ObjectHandle equal to the surface's pointer, and the real services pass it on: CRenderer::offendingObject
 *     is the surface;
 *   - with no current object the handle is null and the real services set a null offending object.
 * Form used (N6): the direct call of the helper. No RSL construct reaches scripterror: the only trigger,
 * "Invalid uniform lighting call" (the DEFLIGHTFUNC macro in execute.cpp), needs a lighting instruction compiled as
 * uniform, and oshader rejects it ("Can not assign to uniform variable"). The error text itself is not
 * produced by this form; scripterror's message and its goto are not touched by the phase.
 * It lives in an executable of its own, EXCLUDE_FROM_ALL until the phase's implementation, because it does not
 * compile before: the helper and the services take an ObjectHandle only from Phase 7 on (that is its red evidence).
 */
#include "case_common.h"
#include "contract_forwarder.h"
#include "rendered_harness.h"

using namespace openrender::shading;

namespace {

    // rendererSetOffendingObject() is protected: a probe context that exposes it, as a shader error would reach it
    class COffendingProbe : public CRProbeContext {
        public:
            explicit COffendingProbe(int thread) : CRProbeContext(thread) {}
            void reportOffender() { rendererSetOffendingObject(objectHandle()); }
    };

} // namespace

int main(int, char **) {
    CRecordingSink sink;
    {
        CRenderedSession s(false);
        CContractForwarder fwd(s.original);

        CShadingContext::setDefaultServices(&fwd);
        COffendingProbe *ctx = new COffendingProbe(0);
        ctx->updateState();

        // A current object: the forwarder records the handle, the real services set the offending object
        ctx->currentShadingState->currentObject = s.object;
        CRenderer::offendingObject = nullptr;
        ctx->reportOffender();
        const void *surface = static_cast<const CObject *>(s.object);
        CT_EXPECT(fwd.offendingObjectCalls == 1);
        CT_EXPECT(fwd.lastOffending == surface);
        CT_EXPECT(static_cast<const void *>(CRenderer::offendingObject) == surface);

        // No current object: a null handle, a null offending object
        ctx->currentShadingState->currentObject = nullptr;
        ctx->reportOffender();
        CT_EXPECT(fwd.offendingObjectCalls == 2);
        CT_EXPECT(fwd.lastOffending == nullptr);
        CT_EXPECT(CRenderer::offendingObject == nullptr);

        delete ctx;
    }
    return contract_test::finish();
}
