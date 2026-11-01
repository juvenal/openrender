/**
 * Project: openRender
 *
 * File: test_concurrent_acquire.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), ProceduresAbi_ConcurrentAcquire (FR-044, SC-018): eight
 *   threads acquire one procedural module at the same moment.
 *
 *   argv[1]: the path of the dlo `legacy` fixture (no version symbol, so the version check has a
 *   message to print).
 *
 *   Required of acquireDloModule (src/procedures/proceduralModules.h):
 *     - every thread gets the same CDloModule: one loaded module per resolved path while it is
 *       referenced (proceduralModuleStats().loaded rises by exactly one);
 *     - the version check runs once: exactly one CODE_VERSION warning in total;
 *     - after the eight references are detached the module is unloaded (unloaded rises by one);
 *     - acquiring it again loads it again but prints nothing (the verdict is cached).
 *
 *   The API does not exist on the starting tree of Phase 3: this program is registered
 *   EXCLUDE_FROM_ALL and DISABLED until T061.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/diagnostics.h"
#include "procedures/proceduralModules.h"

namespace {

    class CCountingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                std::lock_guard<std::mutex> lock(mutex);
                if (code == CODE_VERSION) {
                    ++versionReports;
                    lastSeverity = severity;
                    last = message;
                }
            }
            std::mutex mutex;
            int versionReports = 0;
            EDiagnosticSeverity lastSeverity = DIAGNOSTIC_INFO;
            std::string last;
    };

    int g_failed = 0;
    void expect(bool condition, const char *what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what);
        }
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_procedures_concurrent <dlo legacy module>\n");
        return 2;
    }
    const std::string path = argv[1];
    CCountingSink sink;
    installDiagnosticSink(&sink);

    const SProceduralModuleStats before = proceduralModuleStats();

    const int kThreads = 8;
    std::vector<CDloModule *> got(kThreads, nullptr);
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&, i]() {
            ++ready;
            while (!go.load())
                std::this_thread::yield();
            got[i] = acquireDloModule(path.c_str(), "legacy");
        });
    }
    while (ready.load() < kThreads)
        std::this_thread::yield();
    go = true;
    for (std::thread &t : threads)
        t.join();

    bool allSame = got[0] != nullptr;
    for (CDloModule *m : got)
        allSame = allSame && m == got[0];
    expect(allSame, "eight threads got the same, non-null module");
    expect(proceduralModuleStats().loaded == before.loaded + 1, "one load for eight acquisitions");
    expect(sink.versionReports == 1 && sink.lastSeverity == DIAGNOSTIC_WARNING, "one version message for eight acquisitions");

    for (CDloModule *m : got)
        if (m != nullptr)
            m->detach();
    expect(proceduralModuleStats().unloaded == before.unloaded + 1, "the module is unloaded when the last reference is detached");

    // Again: loaded again, the cached verdict prints nothing.
    CDloModule *again = acquireDloModule(path.c_str(), "legacy");
    expect(again != nullptr && proceduralModuleStats().loaded == before.loaded + 2, "a later acquisition loads it again");
    expect(sink.versionReports == 1, "and prints no second message");
    if (again != nullptr)
        again->detach();

    installDiagnosticSink(nullptr);
    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
