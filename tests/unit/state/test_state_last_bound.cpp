/**
 * Project: openRender
 *
 * File: test_state_last_bound.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), US3 / FR-010, FR-011, FR-024: the
 *   last-bound record (State_LastBound). One assertion per row of
 *   data-model.md's transition table, plus the push/pop, copy and restore
 *   behaviour of the record and the counts of the new bxdf slot.
 *
 *   A State-only program (see test_state_isolation.cpp); no RiBegin session
 *   is open, so the doubles in state_test_doubles.h are legal slot targets.
 *
 *   Red on the starting tree by build failure: EMaterialBinding,
 *   governingMaterial(), bindBxdf() and bxdf() do not exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <utility>

#include "attributes.h"
#include "stateHandles.h"
#include "state_test_doubles.h"

namespace {

    // Releases the test's own reference when a test is done with an object.
    void drop(CCountingShader *object) {
        object->detach();
    }

    void testTransitions() {
        // a fresh block
        {
            CAttributes attributes;
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::None);
        }

        // surface, then bxdf: Bxdf; both handles stay bound
        {
            CCountingShader *s = newShader();
            CCountingShader *b = newShader();
            CAttributes attributes;
            attributes.bindSurface(ShaderGroupRef::adopt(s));
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::Surface);
            attributes.bindBxdf(ShaderGroupRef::adopt(b));
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::Bxdf);
            STATE_CHECK(attributes.surface().get() == s);
            STATE_CHECK(attributes.bxdf().get() == b);
        }

        // bxdf, then surface: Surface; both handles stay bound
        {
            CCountingShader *s = newShader();
            CCountingShader *b = newShader();
            CAttributes attributes;
            attributes.bindBxdf(ShaderGroupRef::adopt(b));
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::Bxdf);
            attributes.bindSurface(ShaderGroupRef::adopt(s));
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::Surface);
            STATE_CHECK(attributes.surface().get() == s);
            STATE_CHECK(attributes.bxdf().get() == b);
        }

        // an empty handle still sets the record (Surface "null" is a binding), and leaves the slot empty
        {
            CAttributes attributes;
            attributes.bindSurface(ShaderGroupRef());
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::Surface);
            STATE_CHECK(!attributes.surface());

            CAttributes other;
            other.bindBxdf(ShaderGroupRef());
            STATE_CHECK(other.governingMaterial() == EMaterialBinding::Bxdf);
            STATE_CHECK(!other.bxdf());

            // an empty bxdf handle over a bound surface: the record says Bxdf, whatever the slots hold
            CCountingShader *s = newShader();
            CAttributes mixed;
            mixed.bindSurface(ShaderGroupRef::adopt(s));
            mixed.bindBxdf(ShaderGroupRef());
            STATE_CHECK(mixed.governingMaterial() == EMaterialBinding::Bxdf);
            STATE_CHECK(mixed.surface().get() == s);
        }

        // displacement, atmosphere, interior and exterior do not touch the record
        {
            CAttributes attributes;
            attributes.bindDisplacement(ShaderGroupRef::adopt(newShader()));
            attributes.bindAtmosphere(ShaderGroupRef::adopt(newShader()));
            attributes.bindInterior(ShaderGroupRef::adopt(newShader()));
            attributes.bindExterior(ShaderGroupRef::adopt(newShader()));
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::None);
        }

        // FR-024: setDefaultSurface stores the handle and leaves the record unchanged
        {
            CCountingShader *d = newShader();
            CAttributes attributes;
            attributes.setDefaultSurface(ShaderGroupRef::adopt(d));
            STATE_CHECK(attributes.surface().get() == d);
            STATE_CHECK(attributes.governingMaterial() == EMaterialBinding::None);

            CCountingShader *b = newShader();
            CCountingShader *d2 = newShader();
            CAttributes over;
            over.bindBxdf(ShaderGroupRef::adopt(b));
            over.setDefaultSurface(ShaderGroupRef::adopt(d2));
            STATE_CHECK(over.surface().get() == d2);
            STATE_CHECK(over.governingMaterial() == EMaterialBinding::Bxdf);
        }
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

    void testPushPop() {
        CCountingShader *outerSurface = newShader();
        CCountingShader *innerBxdf = newShader();
        innerBxdf->attach(); // the test's own reference, to see the inner bxdf go

        CAttributes outer;
        outer.bindSurface(ShaderGroupRef::adopt(outerSurface));
        STATE_CHECK(outer.governingMaterial() == EMaterialBinding::Surface);

        // AttributeBegin: a clone; the clone carries the source's answer
        CAttributes *inner = new CAttributes(&outer);
        STATE_CHECK(inner->governingMaterial() == EMaterialBinding::Surface);
        inner->bindBxdf(ShaderGroupRef::adopt(innerBxdf));
        STATE_CHECK(inner->governingMaterial() == EMaterialBinding::Bxdf);
        STATE_CHECK(outer.governingMaterial() == EMaterialBinding::Surface); // the outer block is untouched
        STATE_CHECK(!outer.bxdf());

        // AttributeEnd: the inner block is destroyed; its bxdf goes with the scope
        delete inner;
        STATE_CHECK(outer.governingMaterial() == EMaterialBinding::Surface);
        STATE_CHECK(!outer.bxdf());
        STATE_CHECK_EQ(refs(*innerBxdf), 1); // only the test's reference is left
        drop(innerBxdf);
        STATE_CHECK_EQ(CCountingShader::live, 1); // the outer surface
    }

    void testRestore() {
        // restore(shading = true) takes the other block's value and replaces the bxdf slot
        {
            CCountingShader *oldBxdf = newShader();
            CCountingShader *newBxdf = newShader();
            oldBxdf->attach();
            newBxdf->attach();

            CAttributes target;
            target.bindBxdf(ShaderGroupRef::adopt(oldBxdf));
            CAttributes other;
            other.bindSurface(ShaderGroupRef::adopt(newShader()));
            other.bindBxdf(ShaderGroupRef::adopt(newBxdf));
            other.bindSurface(ShaderGroupRef()); // record: Surface, bxdf still bound
            STATE_CHECK(other.governingMaterial() == EMaterialBinding::Surface);

            target.restore(&other, 1, 0, 0, 0);
            STATE_CHECK(target.governingMaterial() == EMaterialBinding::Surface);
            STATE_CHECK(target.bxdf().get() == newBxdf);
            STATE_CHECK_EQ(refs(*oldBxdf), 1); // replaced: -1
            STATE_CHECK_EQ(refs(*newBxdf), 3); // test + other + target
            drop(oldBxdf);

            // restore(shading = false) leaves the record and the slots unchanged
            CAttributes keep;
            keep.bindBxdf(ShaderGroupRef());
            STATE_CHECK(keep.governingMaterial() == EMaterialBinding::Bxdf);
            keep.restore(&other, 0, 0, 0, 0);
            STATE_CHECK(keep.governingMaterial() == EMaterialBinding::Bxdf);
            STATE_CHECK(!keep.bxdf());
            target.bindBxdf(ShaderGroupRef());
            other.bindBxdf(ShaderGroupRef());
            drop(newBxdf);
        }
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

    // the bxdf slot's counts: adopt +0, copy +1, restore, destruction -1
    void testBxdfCounts() {
        CCountingShader *b = newShader();
        b->attach(); // the test's own reference; count 2

        CAttributes source;
        source.bindBxdf(ShaderGroupRef::adopt(b));
        STATE_CHECK_EQ(refs(*b), 2); // adopt: +0

        CAttributes *copy = new CAttributes(&source);
        STATE_CHECK_EQ(refs(*b), 3); // copy: +1
        delete copy;
        STATE_CHECK_EQ(refs(*b), 2); // destruction: -1

        CAttributes target;
        target.restore(&source, 1, 0, 0, 0);
        STATE_CHECK_EQ(refs(*b), 3); // restore: +1 in the target
        target.restore(&source, 1, 0, 0, 0);
        STATE_CHECK_EQ(refs(*b), 3); // a second restore of the same object: still one in the target

        target.bindBxdf(ShaderGroupRef());
        source.bindBxdf(ShaderGroupRef());
        STATE_CHECK_EQ(refs(*b), 1);
        drop(b);
    }

} // namespace

int main() {
    printf("transitions\n");
    testTransitions();
    printf("push / pop\n");
    testPushPop();
    printf("restore\n");
    testRestore();
    printf("bxdf slot counts\n");
    testBxdfCounts();
    return stateTestResult("State_LastBound");
}
