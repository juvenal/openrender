/**
 * Project: openRender
 *
 * File: test_state_light_restore.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), FR-025 / SC-010: the direct restore()
 *   and copy-constructor order check (State_LightOrderRestore). The Ri-level
 *   State_LightOrder cannot isolate restore(): its ResourceSave /
 *   ResourceRestore round trip reverses the list twice and so comes out in
 *   order on the starting tree too. This test calls restore() once.
 *
 *   A State-only program (see test_state_isolation.cpp). Lights are counting
 *   test doubles (state_test_doubles.h); entries are non-owning, so no count
 *   changes anywhere.
 *
 *   Red by ASSERTION on the starting tree, once it builds: the copy
 *   constructor and restore(shading) rebuild the list by prepending, which
 *   reverses it. It stays DISABLED until the FR-025 phase (T071) turns it on.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <vector>

#include "attributes.h"
#include "stateHandles.h"
#include "state_test_doubles.h"

namespace {

    std::vector<const CShaderGroupRefTarget *> order(const CAttributes &attributes) {
        std::vector<const CShaderGroupRefTarget *> lights;
        for (const CActiveLight *l = attributes.lightSources; l != nullptr; l = l->next)
            lights.push_back(l->light);
        return lights;
    }

    using TOrder = std::vector<const CShaderGroupRefTarget *>;

    void expectOrder(const char *label, const CAttributes &attributes, const TOrder &expected) {
        const TOrder actual = order(attributes);
        if (actual == expected) {
            ++g_passed;
            printf("  ok   %s\n", label);
            return;
        }
        ++g_failed;
        fprintf(stderr, "  FAIL %s: expected", label);
        for (const CShaderGroupRefTarget *l : expected)
            fprintf(stderr, " %p", static_cast<const void *>(l));
        fprintf(stderr, ", got");
        for (const CShaderGroupRefTarget *l : actual)
            fprintf(stderr, " %p", static_cast<const void *>(l));
        fprintf(stderr, "\n");
    }

} // namespace

int main() {
    CCountingShader *l1 = newShader();
    CCountingShader *l2 = newShader();
    CCountingShader *l3 = newShader();
    CCountingShader *l4 = newShader();

    // block A: L1, L2, L3, added in that order
    CAttributes a;
    a.addLight(l1);
    a.addLight(l2);
    a.addLight(l3);
    expectOrder("addLight appends", a, {l1, l2, l3});

    // the copy constructor lists the same lights in the same order
    CAttributes *copy = new CAttributes(&a);
    expectOrder("copy constructor", *copy, {l1, l2, l3});

    // restore of A into a block B with a different list gives L1, L2, L3
    CAttributes b;
    b.addLight(l4);
    b.addLight(l2);
    expectOrder("block B before restore", b, {l4, l2});
    b.restore(&a, 1, 0, 0, 0);
    expectOrder("restore(shading = true)", b, {l1, l2, l3});

    // restore with shading = false leaves the list alone
    CAttributes c;
    c.addLight(l4);
    c.restore(&a, 0, 0, 0, 0);
    expectOrder("restore(shading = false)", c, {l4});

    // a clone of a clone keeps the order
    CAttributes *grandchild = new CAttributes(copy);
    expectOrder("clone of a clone", *grandchild, {l1, l2, l3});

    // light switching: removeLight L2, then addLight L2, moves it to the end
    a.removeLight(l2);
    expectOrder("removeLight", a, {l1, l3});
    a.addLight(l2);
    expectOrder("light switched off then on", a, {l1, l3, l2});
    CAttributes *afterSwitch = new CAttributes(&a);
    expectOrder("copy after the switch", *afterSwitch, {l1, l3, l2});

    // entries are non-owning: no light's count changed anywhere
    STATE_CHECK_EQ(refs(*l1), 1);
    STATE_CHECK_EQ(refs(*l2), 1);
    STATE_CHECK_EQ(refs(*l3), 1);
    STATE_CHECK_EQ(refs(*l4), 1);

    delete afterSwitch;
    delete grandchild;
    delete copy;
    STATE_CHECK_EQ(CCountingShader::live, 4);
    STATE_CHECK_EQ(refs(*l1), 1);

    for (CAttributes *block : {&a, &b, &c}) {
        block->removeLight(l1);
        block->removeLight(l2);
        block->removeLight(l3);
        block->removeLight(l4);
    }
    l1->detach();
    l2->detach();
    l3->detach();
    l4->detach();
    STATE_CHECK_EQ(CCountingShader::live, 0);

    return stateTestResult("State_LightOrderRestore");
}
