/**
 * Project: openRender
 *
 * File: test_version_check.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), PluginAbi_VersionCheck (US1, FR-003, FR-004, FR-007,
 *   FR-009): checkPluginVersion over a fake module (version_fake.c), every row of data-model.md
 *   section 2.
 *
 *   argv[1]: the path of the fake module.
 *
 *   Each case uses its own interface name, because the verdict is cached per
 *   (interfaceName, realpath(path)) for the process:
 *     absent symbol, warns          AcceptUnversioned, versioned false, treated as 1.0.0: one warning
 *                                   with the path, the interface, "1.0.0" and the release name 1.2.0
 *     absent symbol, dynamicload    Accept as 1.0.0, no diagnostic (FR-009)
 *     MAJOR not supported (2.0.0)   Reject: one error with the path, the interface, the plug-in
 *                                   version as M.m.p and raw hex, the host version, the supported
 *                                   MAJORs and "rebuild against the installed header"
 *     newer MINOR (1.9.0 vs 1.0.0)  AcceptNewerMinor: one warning naming both versions
 *     equal (1.0.0), older (1.1.0 against a host at 1.2.0)   Accept, nothing printed
 *     raw 0x00000000 and 0xFFFFFFFF  Reject, the raw value printed as 0x%08x, no decoding trusted
 *   and the properties around them:
 *     at most one diagnostic per call;
 *     a second and a third load of the same (interface, path) print nothing and return the same
 *     verdict, without calling the module's version symbol again;
 *     a rejection is never retried (the counting symbol is not called again);
 *     the same module under another interface name is checked on its own.
 *
 *   Diagnostics are captured with an installed CDiagnosticSink and must carry CODE_VERSION.
 *   pluginAbi.h does not exist on the starting tree of Phase 2: this program is registered
 *   EXCLUDE_FROM_ALL and DISABLED until T047.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "orplugin.h"
#include "common/os.h"
#include "common/pluginAbi.h"

namespace {

    struct SReport {
            EErrorCode code;
            EDiagnosticSeverity severity;
            std::string message;
    };

    class CRecordingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                reports.push_back({code, severity, message});
            }
            std::vector<SReport> reports;
    };

    CRecordingSink g_sink;
    int g_failed = 0;

    void expect(bool condition, const std::string &what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what.c_str());
        }
    }

    bool contains(const std::string &text, const std::string &part) {
        return text.find(part) != std::string::npos;
    }

    const uint32_t kOnly1_0[] = {OR_ABI_VERSION(1, 0, 0)};
    const uint32_t kOnly1_2[] = {OR_ABI_VERSION(1, 2, 0)};

    SPluginFamily family(const char *interfaceName, const char *symbol, uint32_t host, const uint32_t *supported, bool warns = true) {
        return {interfaceName, symbol, host, supported, 1, warns};
    }

    // One call, with its diagnostics.
    struct SCall {
            SPluginVersionVerdict verdict;
            std::vector<SReport> reports;
    };
    SCall call(const SPluginFamily &f, void *module, const char *path) {
        g_sink.reports.clear();
        SCall c;
        c.verdict = checkPluginVersion(f, module, path);
        c.reports = g_sink.reports;
        return c;
    }

    typedef uint32_t (*CountFunction)(void);

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_plugin_version_check <version_fake module>\n");
        return 2;
    }
    const char *path = argv[1];
    void *module = osLoadModule(path);
    if (module == nullptr) {
        fprintf(stderr, "cannot load %s\n", path);
        return 2;
    }
    auto callCount = (CountFunction)osResolve(module, "orFakeCallCount");
    installDiagnosticSink(&g_sink);

    // --- absent symbol, a family that warns: AcceptUnversioned, one warning that names the release ---
    {
        const SPluginFamily f = family("fake_unversioned", "orFakeNoSuchSymbol", OR_ABI_VERSION(1, 0, 0), kOnly1_0);
        const SCall c = call(f, module, path);
        expect(c.verdict.verdict == EPluginVerdict::AcceptUnversioned && !c.verdict.versioned && c.verdict.pluginVersion == OR_ABI_VERSION(1, 0, 0), "unversioned: AcceptUnversioned, treated as 1.0.0");
        expect(c.reports.size() == 1 && c.reports[0].code == CODE_VERSION && c.reports[0].severity == DIAGNOSTIC_WARNING, "unversioned: exactly one CODE_VERSION warning");
        if (!c.reports.empty()) {
            const std::string &m = c.reports[0].message;
            expect(contains(m, path) && contains(m, "fake_unversioned") && contains(m, "1.0.0") && contains(m, OR_PLUGIN_UNVERSIONED_SUNSET), "unversioned: the warning names path, interface, 1.0.0 and the release " OR_PLUGIN_UNVERSIONED_SUNSET);
        }
        // Cache replay: the second and the third load print nothing and agree.
        for (int again = 2; again <= 3; ++again) {
            const SCall r = call(f, module, path);
            expect(r.reports.empty(), "unversioned: load " + std::to_string(again) + " prints nothing");
            expect(r.verdict.verdict == EPluginVerdict::AcceptUnversioned && !r.verdict.versioned, "unversioned: load " + std::to_string(again) + " returns the cached verdict");
        }
    }

    // --- absent symbol, dynamicload: accepted silently as 1.0.0 (FR-009) ---
    {
        const SPluginFamily f = family("fake_dynamicload", "orFakeNoSuchSymbol", OR_ABI_VERSION(1, 0, 0), kOnly1_0, false);
        const SCall c = call(f, module, path);
        expect(c.verdict.verdict == EPluginVerdict::Accept && !c.verdict.versioned && c.verdict.pluginVersion == OR_ABI_VERSION(1, 0, 0), "dynamicload: Accept as 1.0.0");
        expect(c.reports.empty(), "dynamicload: no diagnostic");
    }

    // --- MAJOR not supported ---
    {
        const SPluginFamily f = family("fake_major", "orFakeV200", OR_ABI_VERSION(1, 0, 0), kOnly1_0);
        const SCall c = call(f, module, path);
        expect(c.verdict.verdict == EPluginVerdict::Reject && c.verdict.versioned && c.verdict.pluginVersion == OR_ABI_VERSION(2, 0, 0), "MAJOR 2 against a host at MAJOR 1: Reject");
        expect(c.reports.size() == 1 && c.reports[0].code == CODE_VERSION && c.reports[0].severity == DIAGNOSTIC_ERROR, "MAJOR 2: exactly one CODE_VERSION error");
        if (!c.reports.empty()) {
            const std::string &m = c.reports[0].message;
            expect(contains(m, path) && contains(m, "fake_major"), "MAJOR 2: the error names the file and the interface");
            expect(contains(m, "2.0.0") && contains(m, "0x00020000"), "MAJOR 2: the plug-in version as M.m.p and as raw hex");
            expect(contains(m, "1.0.0"), "MAJOR 2: the host version");
            expect(contains(m, "rebuild against the installed header"), "MAJOR 2: the remedy");
        }
    }

    // --- newer MINOR ---
    {
        const SPluginFamily f = family("fake_minor", "orFakeV190", OR_ABI_VERSION(1, 0, 0), kOnly1_0);
        const SCall c = call(f, module, path);
        expect(c.verdict.verdict == EPluginVerdict::AcceptNewerMinor && c.verdict.pluginVersion == OR_ABI_VERSION(1, 9, 0), "1.9.0 against 1.0.0: AcceptNewerMinor");
        expect(c.reports.size() == 1 && c.reports[0].severity == DIAGNOSTIC_WARNING && c.reports[0].code == CODE_VERSION, "newer MINOR: exactly one CODE_VERSION warning");
        if (!c.reports.empty()) {
            const std::string &m = c.reports[0].message;
            expect(contains(m, path) && contains(m, "fake_minor") && contains(m, "1.9.0") && contains(m, "1.0.0"), "newer MINOR: path, interface and both versions");
        }
    }

    // --- equal and older MINOR: Accept, nothing printed ---
    {
        const SCall equal = call(family("fake_equal", "orFakeV100", OR_ABI_VERSION(1, 0, 0), kOnly1_0), module, path);
        expect(equal.verdict.verdict == EPluginVerdict::Accept && equal.verdict.versioned && equal.reports.empty(), "equal version: Accept, nothing printed");
        const SCall older = call(family("fake_older", "orFakeV110", OR_ABI_VERSION(1, 2, 0), kOnly1_2), module, path);
        expect(older.verdict.verdict == EPluginVerdict::Accept && older.verdict.pluginVersion == OR_ABI_VERSION(1, 1, 0) && older.reports.empty(), "older MINOR (1.1.0 against 1.2.0): Accept, nothing printed");
    }

    // --- raw garbage is printed raw and rejected ---
    {
        const SCall zero = call(family("fake_raw0", "orFakeRaw0", OR_ABI_VERSION(1, 0, 0), kOnly1_0), module, path);
        expect(zero.verdict.verdict == EPluginVerdict::Reject && zero.verdict.pluginVersion == 0u, "0x00000000: Reject");
        expect(zero.reports.size() == 1 && contains(zero.reports[0].message, "0x00000000"), "0x00000000: one error that prints the raw value");
        const SCall all = call(family("fake_rawffff", "orFakeRawFFFF", OR_ABI_VERSION(1, 0, 0), kOnly1_0), module, path);
        expect(all.verdict.verdict == EPluginVerdict::Reject && all.verdict.pluginVersion == 0xFFFFFFFFu, "0xFFFFFFFF: Reject");
        expect(all.reports.size() == 1 && contains(all.reports[0].message, "0xffffffff"), "0xFFFFFFFF: one error that prints the raw value");
    }

    // --- a rejection is never retried: the version symbol is not called again ---
    {
        const SPluginFamily f = family("fake_counted", "orFakeCounted", OR_ABI_VERSION(1, 0, 0), kOnly1_0);
        const uint32_t before = callCount();
        const SCall first = call(f, module, path);
        expect(first.verdict.verdict == EPluginVerdict::Reject && first.reports.size() == 1, "counted: the first load is rejected with one error");
        expect(callCount() == before + 1, "counted: the version symbol was called once");
        for (int again = 2; again <= 3; ++again) {
            const SCall r = call(f, module, path);
            expect(r.verdict.verdict == EPluginVerdict::Reject && r.reports.empty(), "counted: load " + std::to_string(again) + " is rejected from the cache, silently");
        }
        expect(callCount() == before + 1, "counted: a rejection is never retried (the symbol was not called again)");
    }

    // --- the same module under another interface name is a different key ---
    {
        const SPluginFamily other = family("fake_unversioned_again", "orFakeNoSuchSymbol", OR_ABI_VERSION(1, 0, 0), kOnly1_0);
        const SCall c = call(other, module, path);
        expect(c.reports.size() == 1, "another interface name: checked on its own, one warning");
    }

    installDiagnosticSink(nullptr);
    osUnloadModule(module);

    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
