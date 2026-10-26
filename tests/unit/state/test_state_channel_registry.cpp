/**
 * Project: openRender
 *
 * File: test_state_channel_registry.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), US5 / FR-016 to FR-018, SC-008: the
 *   AOV/channel registry (State_ChannelRegistry), following contracts/
 *   channel-registry.md "Tests".
 *
 *   The declare-from-string path runs through a test-double declaration hook
 *   (installVariableDeclaration), a captureless static function of the
 *   two-argument signature, like the renderer core's adapter. Diagnostics are
 *   captured with an installed CDiagnosticSink.
 *
 *   forEach() callback shape (the contract leaves it open, so this test fixes
 *   it): fn(channel), where channel converts to const CDisplayChannel *.
 *
 *   A State-only program (see test_state_isolation.cpp).
 *
 *   Red on the starting tree by build failure: channelRegistry.h does not
 *   exist, and neither does installVariableDeclaration().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstring>
#include <string>
#include <vector>

#include "channelRegistry.h"
#include "common/diagnostics.h"
#include "common/variable.h"
#include "displayChannel.h"
#include "state_test_doubles.h"
#include "variableLookup.h"

namespace {

    // ---------------------------------------------------------------------------
    // diagnostics capture
    // ---------------------------------------------------------------------------
    class CCaptureSink : public CDiagnosticSink {
        public:
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                codes.push_back(code);
                severities.push_back(severity);
                messages.push_back(message);
            }
            void clear() {
                codes.clear();
                severities.clear();
                messages.clear();
            }

            std::vector<EErrorCode> codes;
            std::vector<EDiagnosticSeverity> severities;
            std::vector<std::string> messages;
    };

    // ---------------------------------------------------------------------------
    // the test-double declaration hook: a captureless static function, like the core's adapter
    // ---------------------------------------------------------------------------
    std::vector<CVariable *> g_declared; // owned by the test

    CVariable *declareHook(const char *name, const char *declaration) {
        CVariable *variable = new CVariable;
        if (parseVariable(variable, name, declaration) == FALSE) {
            delete variable;
            return nullptr;
        }
        g_declared.push_back(variable);
        return variable;
    }

    void freeDeclared() {
        for (CVariable *variable : g_declared)
            delete variable;
        g_declared.clear();
    }

    std::vector<std::string> namesOf(const CChannelRegistry &registry) {
        std::vector<std::string> names;
        registry.forEach([&](const CDisplayChannel *channel) { names.push_back(channel->name); });
        return names;
    }

    void expectBuiltIn(const CChannelRegistry &registry, const char *name, int samples, int offset) {
        const CDisplayChannel *channel = registry.retrieve(name);
        STATE_CHECK(channel != nullptr);
        if (channel == nullptr)
            return;
        STATE_CHECK(strcmp(channel->name, name) == 0);
        STATE_CHECK_EQ(channel->numSamples, samples);
        STATE_CHECK_EQ(channel->outType, offset);
        STATE_CHECK(channel->variable == nullptr);
    }

    // ---------------------------------------------------------------------------
    void testBuiltIns() {
        CChannelRegistry registry;
        expectBuiltIn(registry, "rgb", 3, 0);
        expectBuiltIn(registry, "rgba", 4, 0);
        expectBuiltIn(registry, "a", 1, 3);
        expectBuiltIn(registry, "z", 1, 4);
        expectBuiltIn(registry, "rgbaz", 5, 0);
        STATE_CHECK((namesOf(registry) == std::vector<std::string>{"rgb", "rgba", "a", "z", "rgbaz"}));
        STATE_CHECK(registry.retrieve("nope") == nullptr);
        STATE_CHECK(registry.retrieve("") == nullptr);
    }

    // ---------------------------------------------------------------------------
    void testDeclareFromString() {
        CCaptureSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        installVariableDeclaration(&declareHook);

        CChannelRegistry registry;

        // the same declaration twice gives the same pointer
        CDisplayChannel *first = registry.declare("float myaov");
        STATE_CHECK(first != nullptr);
        CDisplayChannel *second = registry.declare("float myaov");
        STATE_CHECK(second == first);
        STATE_CHECK(registry.retrieve("myaov") == first);
        if (first != nullptr) {
            STATE_CHECK(strcmp(first->name, "myaov") == 0);
            STATE_CHECK_EQ(first->numSamples, 1);
            STATE_CHECK(first->variable != nullptr);
            STATE_CHECK_EQ(first->sampleStart, -1);
        }
        STATE_CHECK_EQ(sink.messages.size(), 0);
        STATE_CHECK_EQ(g_declared.size(), 1); // the second call never reached the hook

        // the same name with a different shape: nullptr and exactly one diagnostic
        sink.clear();
        CDisplayChannel *different = registry.declare("color myaov");
        STATE_CHECK(different == nullptr);
        STATE_CHECK_EQ(sink.messages.size(), 1);
        if (sink.messages.size() == 1) {
            STATE_CHECK(sink.codes[0] == CODE_SYSTEM);
            STATE_CHECK(sink.severities[0] == DIAGNOSTIC_ERROR);
            STATE_CHECK(sink.messages[0].find("Channel \"myaov\" was previously defined differently") != std::string::npos);
        }
        STATE_CHECK(registry.retrieve("myaov") == first); // the first declaration still stands

        installVariableDeclaration(nullptr);
        installDiagnosticSink(previous);
        freeDeclared();
    }

    // with no hook installed, a new string declaration is "ill formed"
    void testNoHook() {
        CCaptureSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        installVariableDeclaration(nullptr);

        CChannelRegistry registry;
        STATE_CHECK(registry.declare("float other") == nullptr);
        STATE_CHECK_EQ(sink.messages.size(), 1);
        if (sink.messages.size() == 1) {
            STATE_CHECK(sink.codes[0] == CODE_SYSTEM);
            STATE_CHECK(sink.messages[0].find("Channel definition \"float other\" is ill formed") != std::string::npos);
        }
        STATE_CHECK(registry.retrieve("other") == nullptr);

        installDiagnosticSink(previous);
    }

    // ---------------------------------------------------------------------------
    void testDeclareFromVariable() {
        CCaptureSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);

        CVariable one;
        CVariable same;
        STATE_CHECK(parseVariable(&one, nullptr, "float shared") == TRUE);
        STATE_CHECK(parseVariable(&same, nullptr, "color shared") == TRUE); // another variable, same name

        CChannelRegistry registry;
        CDisplayChannel *channel = registry.declare(&one);
        STATE_CHECK(channel != nullptr);
        if (channel != nullptr)
            STATE_CHECK(channel->variable == &one);
        STATE_CHECK(registry.declare(&one) == channel); // the same variable returns the same channel
        STATE_CHECK(registry.retrieve("shared") == channel);

        // a different variable under an existing name: nullptr, and no message
        STATE_CHECK(registry.declare(&same) == nullptr);
        STATE_CHECK_EQ(sink.messages.size(), 0);

        installDiagnosticSink(previous);
    }

    // ---------------------------------------------------------------------------
    // out-of-order declarations across two frames, then resetUsage()
    // ---------------------------------------------------------------------------
    void testResetUsage() {
        installVariableDeclaration(&declareHook);
        CChannelRegistry registry;

        // frame 1
        CDisplayChannel *b = registry.declare("float bb");
        CDisplayChannel *a = registry.declare("float aa");
        STATE_CHECK(a != nullptr && b != nullptr);
        if (a == nullptr || b == nullptr) {
            installVariableDeclaration(nullptr);
            freeDeclared();
            return;
        }
        for (const std::string &name : namesOf(registry))
            registry.retrieve(name.c_str())->sampleStart = 7;

        registry.resetUsage();

        // every variable-backed channel is unassigned; the built-ins (no variable) keep their value
        registry.forEach([](const CDisplayChannel *channel) {
            if (channel->variable != nullptr)
                STATE_CHECK_EQ(channel->sampleStart, -1);
            else
                STATE_CHECK_EQ(channel->sampleStart, 7);
        });

        // frame 2: a new channel after the reset; the declaration order never changes
        CDisplayChannel *c = registry.declare("float cc");
        STATE_CHECK(c != nullptr);
        registry.resetUsage();
        STATE_CHECK((namesOf(registry) == std::vector<std::string>{"rgb", "rgba", "a", "z", "rgbaz", "bb", "aa", "cc"}));
        registry.forEach([](const CDisplayChannel *channel) {
            if (channel->variable != nullptr)
                STATE_CHECK_EQ(channel->sampleStart, -1);
        });

        installVariableDeclaration(nullptr);
        freeDeclared();
    }

    // ---------------------------------------------------------------------------
    // the session instance
    // ---------------------------------------------------------------------------
    void testSession() {
        installVariableDeclaration(&declareHook);

        STATE_CHECK(channelRegistry() == nullptr); // nullptr outside a session

        beginChannelRegistry();
        STATE_CHECK(channelRegistry() != nullptr);
        if (channelRegistry() != nullptr) {
            STATE_CHECK_EQ(namesOf(*channelRegistry()).size(), 5);
            STATE_CHECK(channelRegistry()->declare("float sessionaov") != nullptr);
            STATE_CHECK_EQ(namesOf(*channelRegistry()).size(), 6);
        }
        endChannelRegistry();
        STATE_CHECK(channelRegistry() == nullptr);

        // a second session starts fresh: only the five built-ins
        beginChannelRegistry();
        STATE_CHECK(channelRegistry() != nullptr);
        if (channelRegistry() != nullptr) {
            STATE_CHECK((namesOf(*channelRegistry()) == std::vector<std::string>{"rgb", "rgba", "a", "z", "rgbaz"}));
            STATE_CHECK(channelRegistry()->retrieve("sessionaov") == nullptr);
        }
        endChannelRegistry();
        STATE_CHECK(channelRegistry() == nullptr);

        installVariableDeclaration(nullptr);
        freeDeclared();
    }

    // Session balance. An exactly-once-deletion probe is not possible (the registry
    // creates its CDisplayChannels itself), so run several begin / declare / end
    // cycles: each begin yields exactly the five built-ins, each end leaves no
    // instance, and the process completes without a crash or allocator error (a
    // double delete aborts under the allocator's debug aids; see tasks.md T012).
    void testSessionBalance() {
        installVariableDeclaration(&declareHook);

        for (int cycle = 0; cycle < 3; ++cycle) {
            beginChannelRegistry();
            STATE_CHECK(channelRegistry() != nullptr);
            if (channelRegistry() == nullptr)
                break;
            STATE_CHECK_EQ(namesOf(*channelRegistry()).size(), 5);

            const char *declarations[] = {"float balA", "color balB", "vector balC", "float balD"};
            for (const char *declaration : declarations)
                STATE_CHECK(channelRegistry()->declare(declaration) != nullptr);
            STATE_CHECK_EQ(namesOf(*channelRegistry()).size(), 9);

            endChannelRegistry();
            STATE_CHECK(channelRegistry() == nullptr);
            freeDeclared();
        }

        installVariableDeclaration(nullptr);
    }

} // namespace

int main() {
    printf("built-ins\n");
    testBuiltIns();
    printf("declare from a string\n");
    testDeclareFromString();
    printf("declare with no hook\n");
    testNoHook();
    printf("declare from a variable\n");
    testDeclareFromVariable();
    printf("resetUsage\n");
    testResetUsage();
    printf("session\n");
    testSession();
    printf("session balance\n");
    testSessionBalance();
    return stateTestResult("State_ChannelRegistry");
}
