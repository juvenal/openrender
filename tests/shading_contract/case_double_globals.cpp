/**
 * tests/shading_contract/case_double_globals.cpp
 *
 * ShadingContract_DoubleGlobalsMatchRenderer (spec 026, T060; research R10).
 *
 * The services double builds its 29 built-in globals from declarations copied as text from
 * CRenderer::initDeclarations(). This case keeps the copy honest: in a real session it compares the
 * double's table with the renderer's, entry by entry (name, container, type, number of floats and
 * items, entry index, storage), and checks that the renderer's table starts with the same 29.
 */
#include "rendered_cases.h"

#include "case_common.h"
#include "contract_services.h"
#include "rendered_harness.h"

int caseDoubleGlobalsMatchRenderer() {
    CRenderedSession s(false);
    CContractServices svc;
    {
        CRendererServices *real = s.original;
        CT_EXPECT(real != nullptr);
        CT_EXPECT(svc.numGlobalVariables() == (int)VARIABLE_CONSTANTWIDTH + 1);
        CT_EXPECT(real->numGlobalVariables() >= svc.numGlobalVariables());
        const int n = std::min(real->numGlobalVariables(), svc.numGlobalVariables());
        for (int j = 0; j < n; j++) {
            const CVariable *a = svc.globalVariable(j);
            const CVariable *b = real->globalVariable(j);
            if (strcmp(a->name, b->name) != 0)
                fprintf(stderr, "  global %d: the double has %s, the renderer %s\n", j, a->name, b->name);
            CT_EXPECT(strcmp(a->name, b->name) == 0);
            CT_EXPECT(a->container == b->container);
            CT_EXPECT(a->type == b->type);
            CT_EXPECT(a->numFloats == b->numFloats);
            CT_EXPECT(a->numItems == b->numItems);
            CT_EXPECT(a->entry == b->entry);
            CT_EXPECT(a->storage == b->storage);
        }
    }
    return contract_test::finish();
}
