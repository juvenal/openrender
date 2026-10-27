/**
 * Project: openRender
 *
 * File: test_state_pattern_table.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), US4 / FR-013, FR-014, FR-015: the
 *   scoped pattern-node table (State_PatternTable). Scope comes from value
 *   semantics: an attribute push is `new CAttributes(outer)`, so an inner
 *   definition lives in the inner copy and disappears with it.
 *
 *   forEach() callback shape (the contract leaves it open, so this test fixes
 *   it): fn(handle, node), where handle converts to std::string and node
 *   converts to const ShaderGroupRef &.
 *
 *   A State-only program (see test_state_isolation.cpp).
 *
 *   Red on the starting tree by build failure: patternTable.h does not exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <string>
#include <vector>

#include "attributes.h"
#include "common/diagnostics.h"
#include "patternTable.h"
#include "stateHandles.h"
#include "state_test_doubles.h"

namespace {

    // Counts every diagnostic State might write.
    class CCountingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode, EDiagnosticSeverity, const char *) override { ++messages; }
            int messages = 0;
    };

    std::vector<std::string> handlesOf(const CPatternTable &table) {
        std::vector<std::string> handles;
        table.forEach([&](const std::string &handle, const ShaderGroupRef &) { handles.push_back(handle); });
        return handles;
    }

    const CShaderGroupRefTarget *found(const CPatternTable &table, const char *handle) {
        const ShaderGroupRef *node = table.lookup(handle);
        return node != nullptr ? node->get() : nullptr;
    }

    const CShaderGroupRefTarget *found(const CAttributes &attributes, const char *handle) {
        return found(attributes.patterns, handle);
    }

    // US4.1: visible in the block that defined it and in every block cloned later
    void testVisibility() {
        CCountingShader *h = newShader();
        CAttributes outer;
        outer.patterns.define("h", ShaderGroupRef::adopt(h));
        STATE_CHECK(found(outer, "h") == h);

        CAttributes *clone = new CAttributes(&outer);
        STATE_CHECK(found(*clone, "h") == h);
        CAttributes *nested = new CAttributes(clone);
        STATE_CHECK(found(*nested, "h") == h);
        delete nested;
        delete clone;
        STATE_CHECK_EQ(CCountingShader::live, 1);
    }

    // US4.2: a definition made inside a clone is gone with it
    void testScopeEnd() {
        CCountingShader *outerNode = newShader();
        CCountingShader *innerOnly = newShader();
        CCountingShader *shadow = newShader();

        CAttributes outer;
        outer.patterns.define("shared", ShaderGroupRef::adopt(outerNode));

        CAttributes *inner = new CAttributes(&outer);
        inner->patterns.define("only-inside", ShaderGroupRef::adopt(innerOnly));
        inner->patterns.define("shared", ShaderGroupRef::adopt(shadow)); // shadows the outer one
        STATE_CHECK(found(*inner, "only-inside") == innerOnly);
        STATE_CHECK(found(*inner, "shared") == shadow);
        STATE_CHECK(found(outer, "only-inside") == nullptr); // the outer block never saw it
        STATE_CHECK(found(outer, "shared") == outerNode);

        delete inner; // AttributeEnd
        STATE_CHECK(found(outer, "only-inside") == nullptr);
        STATE_CHECK(found(outer, "shared") == outerNode); // the outer definition is found again
        STATE_CHECK_EQ(CCountingShader::live, 1);         // the inner-only and shadow nodes were freed
    }

    // replace in place; position kept
    void testReplaceInPlace() {
        CCountingShader *a = newShader();
        CCountingShader *b = newShader();
        CCountingShader *c = newShader();
        CCountingShader *a2 = newShader();

        CPatternTable table;
        table.define("a", ShaderGroupRef::adopt(a));
        table.define("b", ShaderGroupRef::adopt(b));
        table.define("c", ShaderGroupRef::adopt(c));
        STATE_CHECK_EQ(table.size(), 3);

        table.define("a", ShaderGroupRef::adopt(a2));
        STATE_CHECK_EQ(table.size(), 3); // replaced, not appended
        STATE_CHECK(found(table, "a") == a2);
        const std::vector<std::string> order = handlesOf(table);
        STATE_CHECK(order == (std::vector<std::string>{"a", "b", "c"})); // position kept
        STATE_CHECK_EQ(CCountingShader::live, 3);                        // the replaced node was released

        table.define("d", ShaderGroupRef::retain(a2));
        STATE_CHECK_EQ(refs(*a2), 2);
        STATE_CHECK((handlesOf(table) == std::vector<std::string>{"a", "b", "c", "d"}));
    }

    // never defined: nullptr and no diagnostic (FR-015)
    void testNotFound() {
        CCountingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);

        CPatternTable table;
        STATE_CHECK(table.lookup("nope") == nullptr);
        STATE_CHECK_EQ(table.size(), 0);
        table.define("h", ShaderGroupRef::adopt(newShader()));
        STATE_CHECK(table.lookup("nope") == nullptr);
        STATE_CHECK(table.lookup("") == nullptr);

        CAttributes attributes;
        STATE_CHECK(attributes.patterns.lookup("nope") == nullptr);

        installDiagnosticSink(previous);
        STATE_CHECK_EQ(sink.messages, 0);
    }

    // forEach: definition order, unique handles, size() matches
    void testForEach() {
        CPatternTable table;
        const char *names[] = {"z", "m", "a", "m", "q", "z"};
        for (const char *n : names)
            table.define(n, ShaderGroupRef::adopt(newShader()));

        std::vector<std::string> seen;
        int count = 0;
        table.forEach([&](const std::string &handle, const ShaderGroupRef &node) {
            seen.push_back(handle);
            STATE_CHECK(static_cast<bool>(node));
            ++count;
        });
        STATE_CHECK((seen == std::vector<std::string>{"z", "m", "a", "q"})); // first-definition order, unique
        STATE_CHECK_EQ(count, 4);
        STATE_CHECK_EQ(table.size(), 4);
    }

    // copy-on-write
    void testCopyOnWrite() {
        CCountingShader *x = newShader();
        CCountingShader *onlyInClone = newShader();
        CCountingShader *laterInSource = newShader();

        CAttributes source;
        source.patterns.define("x", ShaderGroupRef::adopt(x));

        CAttributes *clone = new CAttributes(&source);
        clone->patterns.define("clone-only", ShaderGroupRef::adopt(onlyInClone));
        STATE_CHECK(found(source, "clone-only") == nullptr); // a define in the clone does not change the source
        STATE_CHECK(found(*clone, "x") == x);

        source.patterns.define("later", ShaderGroupRef::adopt(laterInSource));
        STATE_CHECK(found(*clone, "later") == nullptr); // a later define in the source does not change the clone
        STATE_CHECK(found(source, "later") == laterInSource);
        STATE_CHECK_EQ(source.patterns.size(), 2);
        STATE_CHECK_EQ(clone->patterns.size(), 2);

        // replacing in the clone leaves the source's entry alone
        CCountingShader *replacement = newShader();
        clone->patterns.define("x", ShaderGroupRef::adopt(replacement));
        STATE_CHECK(found(source, "x") == x);
        STATE_CHECK(found(*clone, "x") == replacement);

        delete clone;
        STATE_CHECK(found(source, "x") == x);
    }

    // restore(shading)
    void testRestore() {
        CCountingShader *fromOther = newShader();
        CCountingShader *own = newShader();

        CAttributes other;
        other.patterns.define("o", ShaderGroupRef::adopt(fromOther));
        CAttributes target;
        target.patterns.define("t", ShaderGroupRef::adopt(own));

        target.restore(&other, 0, 0, 0, 0); // shading = false: the table is left alone
        STATE_CHECK(found(target, "t") == own);
        STATE_CHECK(found(target, "o") == nullptr);

        target.restore(&other, 1, 0, 0, 0); // shading = true: the table is replaced with the other block's
        STATE_CHECK(found(target, "o") == fromOther);
        STATE_CHECK(found(target, "t") == nullptr);
        STATE_CHECK_EQ(target.patterns.size(), 1);
        STATE_CHECK_EQ(CCountingShader::live, 1); // the node only the old table held was released
    }

    // US4.4: a node bound into a slot outlives its defining block
    void testNodeOutlivesScope() {
        CCountingShader *node = newShader(); // count 1: the table's
        CAttributes consumer;
        {
            CAttributes *defining = new CAttributes;
            defining->patterns.define("surfaceNode", ShaderGroupRef::adopt(node));
            const ShaderGroupRef *entry = defining->patterns.lookup("surfaceNode");
            STATE_CHECK(entry != nullptr);
            consumer.bindSurface(*entry); // the slot shares the reference
            STATE_CHECK_EQ(refs(*node), 2);
            delete defining; // the defining scope ends
        }
        STATE_CHECK_EQ(CCountingShader::live, 1); // the node is still alive
        STATE_CHECK_EQ(refs(*node), 1);           // through the slot's own reference
        STATE_CHECK(consumer.surface().get() == node);

        consumer.bindSurface(ShaderGroupRef());
        STATE_CHECK_EQ(CCountingShader::live, 0);
    }

} // namespace

int main() {
    printf("visibility\n");
    testVisibility();
    printf("scope end\n");
    testScopeEnd();
    printf("replace in place\n");
    testReplaceInPlace();
    printf("not found\n");
    testNotFound();
    printf("forEach\n");
    testForEach();
    printf("copy on write\n");
    testCopyOnWrite();
    printf("restore\n");
    testRestore();
    printf("node outlives its scope\n");
    testNodeOutlivesScope();
    return stateTestResult("State_PatternTable");
}
