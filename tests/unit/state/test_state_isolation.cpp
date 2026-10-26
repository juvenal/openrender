/**
 * Project: openRender
 *
 * File: test_state_isolation.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), US1 / FR-001 to FR-004: the handle and
 *   lifetime test (State_Isolation). A State-only program: it compiles the
 *   State sources on a deliberately narrow include path and links only
 *   libRIBCommon, so it also proves the State headers include no other
 *   domain's header.
 *
 *   Every row of data-model.md "Ownership mapping" is asserted as a
 *   reference-count delta on a counting test double (state_test_doubles.h).
 *   The doubles are not CShaderInstance objects. That is only legal because
 *   this program never opens an RiBegin session, so no shader parameter hook
 *   is installed and nothing casts a slot's target (contracts/
 *   shader-parameter-hooks.md, "Precondition").
 *
 *   Red on the starting tree by build failure: stateHandles.h does not exist,
 *   and attributes.h still includes shader.h, which is not on this include
 *   path.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <type_traits>
#include <utility>

#include "attributes.h"
#include "options.h"
#include "stateHandles.h"
#include "state_test_doubles.h"

namespace {

    // One row per shader slot: how to bind it and how to read it back.
    struct SSlot {
            const char *name;
            void (CAttributes::*bind)(ShaderGroupRef);
            const ShaderGroupRef &(CAttributes::*get)() const;
    };

    const SSlot kSlots[] = {
        {"surface", &CAttributes::bindSurface, &CAttributes::surface},
        {"displacement", &CAttributes::bindDisplacement, &CAttributes::displacement},
        {"atmosphere", &CAttributes::bindAtmosphere, &CAttributes::atmosphere},
        {"interior", &CAttributes::bindInterior, &CAttributes::interior},
        {"exterior", &CAttributes::bindExterior, &CAttributes::exterior},
    };
    const int kNumSlots = 5;

    int lightEntries(const CAttributes &attributes) {
        int n = 0;
        for (const CActiveLight *l = attributes.lightSources; l != nullptr; l = l->next)
            ++n;
        return n;
    }

    // ---------------------------------------------------------------------------
    // TStateRef
    // ---------------------------------------------------------------------------
    void testHandle() {
        static_assert(!std::is_convertible_v<ShaderGroupRef, MapRef>, "ShaderGroupRef must not convert to MapRef");
        static_assert(!std::is_convertible_v<MapRef, ShaderGroupRef>, "MapRef must not convert to ShaderGroupRef");
        static_assert(!std::is_convertible_v<ShaderGroupRef, bool>, "operator bool must be explicit");

        // empty handles
        ShaderGroupRef empty;
        STATE_CHECK(!empty);
        STATE_CHECK(empty.get() == nullptr);
        STATE_CHECK(!ShaderGroupRef::adopt(nullptr));
        STATE_CHECK(!ShaderGroupRef::retain(nullptr));
        STATE_CHECK(!MapRef::adopt(nullptr));
        STATE_CHECK(!MapRef::retain(nullptr));

        // adopt takes over the caller's reference, retain adds one
        CCountingShader *p = newShader();
        STATE_CHECK_EQ(refs(*p), 1);
        {
            ShaderGroupRef a = ShaderGroupRef::adopt(p);
            STATE_CHECK(static_cast<bool>(a));
            STATE_CHECK(a.get() == p);
            STATE_CHECK_EQ(refs(*p), 1);

            // copy: +1
            ShaderGroupRef b(a);
            STATE_CHECK_EQ(refs(*p), 2);

            // move: no change, the source is left empty
            ShaderGroupRef c(std::move(b));
            STATE_CHECK_EQ(refs(*p), 2);
            STATE_CHECK(!b);
            STATE_CHECK(c.get() == p);

            // retain: +1
            {
                ShaderGroupRef r = ShaderGroupRef::retain(p);
                STATE_CHECK_EQ(refs(*p), 3);
            }
            STATE_CHECK_EQ(refs(*p), 2);

            // comparison against a handle and against a raw pointer
            const CShaderGroupRefTarget *raw = p;
            STATE_CHECK(a == c);
            STATE_CHECK(!(a != c));
            STATE_CHECK(a == raw);
            STATE_CHECK(!(a != raw));
            STATE_CHECK(a != empty);
            STATE_CHECK(empty == static_cast<const CShaderGroupRefTarget *>(nullptr));

            // reset releases one count
            c.reset();
            STATE_CHECK(!c);
            STATE_CHECK_EQ(refs(*p), 1);
        }
        STATE_CHECK_EQ(CCountingShader::live, 0); // the last handle's destructor freed it

        // assignment retains the new object before it releases the old one
        CCountingShader *x = newShader();
        CCountingShader *y = newShader();
        {
            ShaderGroupRef h = ShaderGroupRef::adopt(x);
            ShaderGroupRef other = ShaderGroupRef::adopt(y);
            h = other; // x loses its only reference and is freed; y gains one
            STATE_CHECK_EQ(CCountingShader::live, 1);
            STATE_CHECK_EQ(refs(*y), 2);
            STATE_CHECK(h == other);

            // self-assignment is safe even when the handle holds the only reference
            CCountingShader *z = newShader();
            ShaderGroupRef solo = ShaderGroupRef::adopt(z);
            const ShaderGroupRef &alias = solo;
            solo = alias;
            STATE_CHECK_EQ(CCountingShader::live, 2); // z survived
            STATE_CHECK_EQ(refs(*z), 1);
        }
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

    // ---------------------------------------------------------------------------
    // adopt into each slot; replace; destruction
    // ---------------------------------------------------------------------------
    void testAdoptReplaceDestroy() {
        for (int i = 0; i < kNumSlots; ++i) {
            const SSlot &slot = kSlots[i];
            printf("  slot %s\n", slot.name);

            CAttributes attributes;

            // adopt: the bind leaves the count alone
            CCountingShader *p = newShader();
            p->attach(); // the test keeps one reference of its own, to observe the object
            STATE_CHECK_EQ(refs(*p), 2);
            (attributes.*slot.bind)(ShaderGroupRef::adopt(p)); // adopts the constructor's reference
            STATE_CHECK_EQ(refs(*p), 2);
            STATE_CHECK((attributes.*slot.get)().get() == p);

            // replace: the old object loses exactly one reference
            CCountingShader *q = newShader();
            (attributes.*slot.bind)(ShaderGroupRef::adopt(q));
            STATE_CHECK_EQ(refs(*p), 1);
            STATE_CHECK((attributes.*slot.get)().get() == q);

            // a destroyed object is freed: the test's own reference is the last one
            STATE_CHECK_EQ(CCountingShader::live, 2);
            p->detach();
            STATE_CHECK_EQ(CCountingShader::live, 1);

            // an empty handle releases the slot
            (attributes.*slot.bind)(ShaderGroupRef());
            STATE_CHECK(!(attributes.*slot.get)());
            STATE_CHECK_EQ(CCountingShader::live, 0);
        }

        // destruction of the block releases every slot
        CCountingShader *held[kNumSlots];
        {
            CAttributes attributes;
            for (int i = 0; i < kNumSlots; ++i) {
                held[i] = newShader();
                held[i]->attach(); // the test's own reference
                (attributes.*kSlots[i].bind)(ShaderGroupRef::adopt(held[i]));
                STATE_CHECK_EQ(refs(*held[i]), 2);
            }
        }
        for (int i = 0; i < kNumSlots; ++i) {
            STATE_CHECK_EQ(refs(*held[i]), 1);
            held[i]->detach();
        }
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

    // ---------------------------------------------------------------------------
    // the copy constructor: +1 on each of the five slots and both maps
    // ---------------------------------------------------------------------------
    void testCopyConstructor() {
        CCountingShader *shaders[kNumSlots];
        CCountingMap *globalMap = newMap();
        CCountingMap *causticMap = newMap();

        CAttributes source;
        for (int i = 0; i < kNumSlots; ++i) {
            shaders[i] = newShader();
            (source.*kSlots[i].bind)(ShaderGroupRef::adopt(shaders[i]));
            shaders[i]->attach(); // the test's own reference; count 2
        }
        source.globalMap = MapRef::retain(globalMap);   // count 2
        source.causticMap = MapRef::retain(causticMap); // count 2

        for (int i = 0; i < kNumSlots; ++i)
            STATE_CHECK_EQ(refs(*shaders[i]), 2);

        CAttributes *clone = new CAttributes(&source);

        // exactly +1 each; a clone that changes a count by anything else is the doubled-attach bug
        for (int i = 0; i < kNumSlots; ++i) {
            STATE_CHECK_EQ(refs(*shaders[i]), 3);
            STATE_CHECK((clone->*kSlots[i].get)().get() == shaders[i]);
        }
        STATE_CHECK_EQ(refs(*globalMap), 3);
        STATE_CHECK_EQ(refs(*causticMap), 3);
        STATE_CHECK(clone->globalMap.get() == globalMap);
        STATE_CHECK(clone->causticMap.get() == causticMap);

        // the clone's destruction gives exactly -1
        delete clone;
        for (int i = 0; i < kNumSlots; ++i)
            STATE_CHECK_EQ(refs(*shaders[i]), 2);
        STATE_CHECK_EQ(refs(*globalMap), 2);
        STATE_CHECK_EQ(refs(*causticMap), 2);

        // unbinding the source's surface leaves the test's own reference as the only one
        source.bindSurface(ShaderGroupRef());
        STATE_CHECK_EQ(refs(*shaders[0]), 1);
        source.bindSurface(ShaderGroupRef::retain(shaders[0]));
        STATE_CHECK_EQ(refs(*shaders[0]), 2);

        // a block that is a copy of a copy
        CAttributes *first = new CAttributes(&source);
        CAttributes *second = new CAttributes(first);
        STATE_CHECK_EQ(refs(*shaders[1]), 4);
        delete first;
        STATE_CHECK_EQ(refs(*shaders[1]), 3);
        delete second;
        STATE_CHECK_EQ(refs(*shaders[1]), 2);

        source.globalMap.reset();
        source.causticMap.reset();
        STATE_CHECK_EQ(refs(*globalMap), 1);
        STATE_CHECK_EQ(refs(*causticMap), 1);
        globalMap->detach();
        causticMap->detach();
        for (int i = 0; i < kNumSlots; ++i)
            (source.*kSlots[i].bind)(ShaderGroupRef());
        for (int i = 0; i < kNumSlots; ++i)
            shaders[i]->detach();
        STATE_CHECK_EQ(CCountingShader::live, 0);
        STATE_CHECK_EQ(CCountingMap::live, 0);
    }

    // ---------------------------------------------------------------------------
    // restore(shading = true): the five slots are replaced; the maps are untouched
    // ---------------------------------------------------------------------------
    void testRestore() {
        CCountingShader *fromSource[kNumSlots];
        CCountingShader *fromTarget[kNumSlots];
        CCountingMap *targetGlobal = newMap();
        CCountingMap *sourceGlobal = newMap();

        CAttributes source;
        CAttributes target;
        for (int i = 0; i < kNumSlots; ++i) {
            fromSource[i] = newShader();
            (source.*kSlots[i].bind)(ShaderGroupRef::adopt(fromSource[i]));
            fromSource[i]->attach();
            fromTarget[i] = newShader();
            (target.*kSlots[i].bind)(ShaderGroupRef::adopt(fromTarget[i]));
            fromTarget[i]->attach();
        }
        target.globalMap = MapRef::retain(targetGlobal); // count 2
        source.globalMap = MapRef::retain(sourceGlobal); // count 2

        target.restore(&source, 1, 0, 0, 0);

        for (int i = 0; i < kNumSlots; ++i) {
            STATE_CHECK_EQ(refs(*fromTarget[i]), 1); // old: -1
            STATE_CHECK_EQ(refs(*fromSource[i]), 3); // new: +1
            STATE_CHECK((target.*kSlots[i].get)().get() == fromSource[i]);
        }

        // restore copies only the map names: the two map handles are untouched
        STATE_CHECK(target.globalMap.get() == targetGlobal);
        STATE_CHECK_EQ(refs(*targetGlobal), 2);
        STATE_CHECK_EQ(refs(*sourceGlobal), 2);

        // shading = false leaves the slots alone
        CAttributes other;
        CCountingShader *unrelated = newShader();
        other.bindSurface(ShaderGroupRef::adopt(unrelated));
        target.restore(&other, 0, 0, 0, 0);
        STATE_CHECK((target.*kSlots[0].get)().get() == fromSource[0]);
        STATE_CHECK_EQ(refs(*unrelated), 1);

        target.globalMap.reset();
        source.globalMap.reset();
        targetGlobal->detach();
        sourceGlobal->detach();
        for (int i = 0; i < kNumSlots; ++i) {
            (target.*kSlots[i].bind)(ShaderGroupRef());
            (source.*kSlots[i].bind)(ShaderGroupRef());
            fromSource[i]->detach();
            fromTarget[i]->detach();
        }
        other.bindSurface(ShaderGroupRef());
        STATE_CHECK_EQ(CCountingShader::live, 0);
        STATE_CHECK_EQ(CCountingMap::live, 0);
    }

    // ---------------------------------------------------------------------------
    // light entries are non-owning
    // ---------------------------------------------------------------------------
    void testLights() {
        CCountingShader *l1 = newShader(); // count 1: the test's reference
        CCountingShader *l2 = newShader();

        CAttributes attributes;
        attributes.addLight(l1);
        STATE_CHECK_EQ(refs(*l1), 1);
        attributes.addLight(l1); // the same light twice keeps one entry
        STATE_CHECK_EQ(lightEntries(attributes), 1);
        attributes.addLight(l2);
        STATE_CHECK_EQ(lightEntries(attributes), 2);
        STATE_CHECK_EQ(refs(*l1), 1);
        STATE_CHECK_EQ(refs(*l2), 1);

        CAttributes *clone = new CAttributes(&attributes);
        STATE_CHECK_EQ(lightEntries(*clone), 2);
        STATE_CHECK_EQ(refs(*l1), 1);
        STATE_CHECK_EQ(refs(*l2), 1);

        CAttributes restored;
        restored.restore(&attributes, 1, 0, 0, 0);
        STATE_CHECK_EQ(lightEntries(restored), 2);
        STATE_CHECK_EQ(refs(*l1), 1);
        STATE_CHECK_EQ(refs(*l2), 1);

        clone->removeLight(l1);
        STATE_CHECK_EQ(lightEntries(*clone), 1);
        STATE_CHECK_EQ(lightEntries(attributes), 2);
        STATE_CHECK_EQ(refs(*l1), 1);

        delete clone; // destruction frees the entries, not the lights
        STATE_CHECK_EQ(CCountingShader::live, 2);
        STATE_CHECK_EQ(refs(*l1), 1);
        STATE_CHECK_EQ(refs(*l2), 1);

        // the lights outlive every block that listed them
        attributes.removeLight(l1);
        attributes.removeLight(l2);
        STATE_CHECK_EQ(lightEntries(attributes), 0);
        restored.removeLight(l1);
        restored.removeLight(l2);
        l1->detach();
        l2->detach();
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

    // ---------------------------------------------------------------------------
    // COptions::imager
    // ---------------------------------------------------------------------------
    void testImager() {
        CCountingShader *imager = newShader();
        imager->attach(); // the test's own reference

        {
            COptions options;
            options.imager = ShaderGroupRef::adopt(imager);
            STATE_CHECK_EQ(refs(*imager), 2);

            COptions *copy = new COptions(&options);
            STATE_CHECK_EQ(refs(*imager), 3);
            STATE_CHECK(copy->imager.get() == imager);
            delete copy;
            STATE_CHECK_EQ(refs(*imager), 2);
        }
        STATE_CHECK_EQ(refs(*imager), 1);
        imager->detach();
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

    // ---------------------------------------------------------------------------
    // the motion chain
    // ---------------------------------------------------------------------------
    void testMotionChain() {
        CCountingShader *head = newShader();
        CCountingShader *tail = newShader();
        head->attach();
        tail->attach();

        CAttributes source;
        source.bindSurface(ShaderGroupRef::adopt(head));
        source.next = new CAttributes;
        source.next->bindSurface(ShaderGroupRef::adopt(tail));
        STATE_CHECK_EQ(refs(*head), 2);
        STATE_CHECK_EQ(refs(*tail), 2);

        CAttributes *clone = new CAttributes(&source);

        // the chain is copied recursively, and each block keeps its own handles
        STATE_CHECK(clone->next != nullptr);
        STATE_CHECK(clone->next != source.next);
        STATE_CHECK(clone->surface().get() == head);
        STATE_CHECK(clone->next->surface().get() == tail);
        STATE_CHECK_EQ(refs(*head), 3);
        STATE_CHECK_EQ(refs(*tail), 3);

        delete clone; // deletes the copied chain too
        STATE_CHECK_EQ(refs(*head), 2);
        STATE_CHECK_EQ(refs(*tail), 2);

        source.bindSurface(ShaderGroupRef());
        source.next->bindSurface(ShaderGroupRef());
        head->detach();
        tail->detach();
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

    // ---------------------------------------------------------------------------
    // no hook installed, and volume-shader selection
    // ---------------------------------------------------------------------------
    void testNoHookAndSelection() {
        CAttributes attributes;
        for (int i = 0; i < kNumSlots; ++i) {
            (attributes.*kSlots[i].bind)(ShaderGroupRef::adopt(newShader()));
            // no RiBegin session in this program: no parameter hook is installed
            STATE_CHECK_EQ(attributes.usedParameters, 0);
            STATE_CHECK(attributes.findParameter("x") == nullptr);
        }
        attributes.addLight(attributes.surface().get());
        STATE_CHECK_EQ(attributes.usedParameters, 0);
        attributes.removeLight(attributes.surface().get());

        // selectVolumeShader returns the interior or exterior slot's target
        STATE_CHECK(selectVolumeShader(&attributes, true, false) == attributes.interior().get());
        STATE_CHECK(selectVolumeShader(&attributes, true, true) == attributes.exterior().get());
        STATE_CHECK(selectVolumeShader(&attributes, false, true) == nullptr);
        STATE_CHECK(selectVolumeShader(&attributes, false, false) == nullptr);

        CAttributes bare;
        STATE_CHECK(selectVolumeShader(&bare, true, true) == nullptr);
        STATE_CHECK(selectVolumeShader(&bare, true, false) == nullptr);

        for (int i = 0; i < kNumSlots; ++i)
            (attributes.*kSlots[i].bind)(ShaderGroupRef());
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

} // namespace

int main() {
    printf("handle\n");
    testHandle();
    printf("adopt / replace / destroy\n");
    testAdoptReplaceDestroy();
    printf("copy constructor\n");
    testCopyConstructor();
    printf("restore\n");
    testRestore();
    printf("lights\n");
    testLights();
    printf("imager\n");
    testImager();
    printf("motion chain\n");
    testMotionChain();
    printf("no hook, volume selection\n");
    testNoHookAndSelection();
    return stateTestResult("State_Isolation");
}
