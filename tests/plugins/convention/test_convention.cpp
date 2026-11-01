/**
 * Project: openRender
 *
 * File: test_convention.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), PluginAbi_Convention (US6, SC-010): the shared plug-in
 *   convention end to end, over the four conv_* plug-ins (conv_fixture.c).
 *
 *   argv: the paths of conv_current, conv_larger, conv_smaller and conv_major2, in that order.
 *
 *   Checks, through checkPluginVersion and checkPluginTable (src/common/pluginAbi.h):
 *     - a table of the required size (current) and a larger one (larger, version 1.1.0 with one
 *       entry more) are accepted; the larger one's newer MINOR is a warning, not a rejection;
 *     - a table below the required size (smaller) is rejected with a diagnostic that names the
 *       path, the interface and both sizes, and the host never reads it past header.structSize: the
 *       table sits at the end of a page followed by a PROT_NONE page, so a read past the declared
 *       size kills this program with a signal;
 *     - MAJOR 2 (major2) is rejected by the version check: the host supports MAJOR 1 only;
 *     - a table whose MAJOR differs from the version symbol's is rejected;
 *     - orplugin::makeTable<ConvPlugin> equals, byte for byte (memcmp), the C initializer the
 *       conv_current plug-in uses (US6-3).
 *
 *   The API (pluginAbi.h, orplugin.h, orplugin.hpp) does not exist on the starting tree of Phase 2:
 *   this program is registered EXCLUDE_FROM_ALL and DISABLED until T047.
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
#include <cstring>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "orplugin.hpp"
#include "common/os.h"
#include "common/pluginAbi.h"
#include "conv_table.h"

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

    void expect(bool condition, const char *what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what);
        }
    }

    bool contains(const std::string &text, const std::string &part) {
        return text.find(part) != std::string::npos;
    }

    const uint32_t kSupported[] = {OR_ABI_VERSION(1, 0, 0)};
    const SPluginFamily kFamily = {"conv", "orConvAbiVersion", OR_ABI_VERSION(1, 0, 0), kSupported, 1, true};

    const ConvPlugin *registerPlugin(void *module, const OrHostInfo &host) {
        auto entry = (ConvRegisterFunction)osResolve(module, "orConvPlugin");
        return entry == nullptr ? nullptr : (const ConvPlugin *)entry(&host);
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: test_convention <conv_current> <conv_larger> <conv_smaller> <conv_major2>\n");
        return 2;
    }
    const char *paths[4] = {argv[1], argv[2], argv[3], argv[4]};
    void *modules[4];
    for (int i = 0; i < 4; ++i) {
        modules[i] = osLoadModule(paths[i]);
        if (modules[i] == nullptr) {
            fprintf(stderr, "cannot load %s\n", paths[i]);
            return 2;
        }
    }
    installDiagnosticSink(&g_sink);

    OrHostInfo host = {sizeof(OrHostInfo), OR_ABI_VERSION(1, 0, 0), "openRender", "test"};
    const size_t required = sizeof(ConvPlugin);

    // --- current: the required size, accepted, no diagnostic; byte-identical to makeTable ---
    {
        g_sink.reports.clear();
        const SPluginVersionVerdict v = checkPluginVersion(kFamily, modules[0], paths[0]);
        expect(v.verdict == EPluginVerdict::Accept && v.versioned && v.pluginVersion == OR_ABI_VERSION(1, 0, 0), "current: accepted as 1.0.0");
        const ConvPlugin *table = registerPlugin(modules[0], host);
        expect(table != nullptr, "current: the registration entry point returns a table");
        if (table != nullptr) {
            expect(checkPluginTable(kFamily, paths[0], &table->header, required, v.pluginVersion), "current: a table of the required size is accepted");
            const ConvPlugin made = orplugin::makeTable<ConvPlugin>(OR_ABI_VERSION(1, 0, 0));
            expect(std::memcmp(table, &made, sizeof(ConvPlugin)) == 0, "current: the C initializer equals orplugin::makeTable<ConvPlugin> byte for byte");
            expect(made.header.structSize == sizeof(ConvPlugin) && made.first == nullptr && made.second == nullptr, "makeTable: sizes the header and leaves every entry null");
            // A table whose MAJOR differs from the version symbol's is rejected.
            g_sink.reports.clear();
            expect(!checkPluginTable(kFamily, paths[0], &table->header, required, OR_ABI_VERSION(2, 0, 0)), "table MAJOR 1 against a symbol MAJOR 2 is rejected");
            expect(!g_sink.reports.empty(), "the MAJOR mismatch is diagnosed");
        }
    }

    // --- larger: version 1.1.0, one entry more; a newer MINOR is a warning, the table is accepted ---
    {
        g_sink.reports.clear();
        const SPluginVersionVerdict v = checkPluginVersion(kFamily, modules[1], paths[1]);
        expect(v.verdict == EPluginVerdict::AcceptNewerMinor && v.pluginVersion == OR_ABI_VERSION(1, 1, 0), "larger: accepted with a newer-MINOR verdict");
        expect(g_sink.reports.size() == 1 && g_sink.reports[0].code == CODE_VERSION && g_sink.reports[0].severity == DIAGNOSTIC_WARNING, "larger: exactly one CODE_VERSION warning");
        const ConvPlugin *table = registerPlugin(modules[1], host);
        expect(table != nullptr && table->header.structSize == sizeof(ConvPluginV11), "larger: the table is the larger one");
        if (table != nullptr)
            expect(checkPluginTable(kFamily, paths[1], &table->header, required, v.pluginVersion), "larger: a table larger than required is accepted");
    }

    // --- smaller: below the required size, at the end of a page; rejected without reading on ---
    {
        g_sink.reports.clear();
        const SPluginVersionVerdict v = checkPluginVersion(kFamily, modules[2], paths[2]);
        expect(v.verdict == EPluginVerdict::Accept, "smaller: the version itself is acceptable");
        const ConvPlugin *table = registerPlugin(modules[2], host);
        expect(table != nullptr, "smaller: the registration entry point returns a table");
        if (table != nullptr) {
            const bool accepted = checkPluginTable(kFamily, paths[2], &table->header, required, v.pluginVersion);
            expect(!accepted, "smaller: a table below the required size is rejected");
            // Reaching this line means the host did not touch the guard page.
            expect(g_sink.reports.size() == 1 && g_sink.reports[0].severity == DIAGNOSTIC_ERROR, "smaller: exactly one error");
            if (!g_sink.reports.empty()) {
                const std::string &text = g_sink.reports[0].message;
                expect(contains(text, paths[2]) && contains(text, "conv"), "smaller: the error names the path and the interface");
                expect(contains(text, std::to_string(table->header.structSize)) && contains(text, std::to_string(required)), "smaller: the error names structSize and the required size");
            }
        }
    }

    // --- major2: the host supports MAJOR 1 only ---
    {
        g_sink.reports.clear();
        const SPluginVersionVerdict v = checkPluginVersion(kFamily, modules[3], paths[3]);
        expect(v.verdict == EPluginVerdict::Reject && v.pluginVersion == OR_ABI_VERSION(2, 0, 0), "major2: MAJOR 2 is rejected");
        expect(g_sink.reports.size() == 1 && g_sink.reports[0].severity == DIAGNOSTIC_ERROR && g_sink.reports[0].code == CODE_VERSION, "major2: exactly one CODE_VERSION error");
    }

    installDiagnosticSink(nullptr);
    for (int i = 0; i < 4; ++i)
        osUnloadModule(modules[i]);

    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
