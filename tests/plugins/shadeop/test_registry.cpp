/**
 * Project: openRender
 *
 * File: test_registry.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), Phase 4: the shadeop module tests that call its API
 *   (src/shadeop/shadeopModule.h, contracts/shadeop.md section 3). One program, one mode per test:
 *
 *   versions     ShadeopAbi_Versions_Registry (SC-001, SC-002): the cases of the render-level
 *                ShadeopAbi_Versions_Render through CShadeopRegistry::acquire. good and hidden: no
 *                diagnostic; newer 1.9.0: exactly one warning; v2: acquire returns NULL with exactly
 *                one error (file, interface, both versions, remedy) and no entry point of the module
 *                is called; v2 searched before good: the scan goes on; legacy: one warning across the
 *                two operands of the module.
 *   perthread    ShadeopAbi_PerThread (SC-007, US4-1): good on N = 8 threads: N distinct data
 *                pointers, each stable per thread index, N threadCleanup calls after the registry is
 *                released, every threadCleanup before cleanup and cleanup before unload.
 *   lifetime     ShadeopAbi_ThreadLifetime (FR-011 b to f, SC-007): threadInit lazily and once per
 *                thread index with concurrent first calls; a NULL result passed through (and still
 *                cleaned up); slots grow when numThreads rises between frames, existing data kept,
 *                never shrunk; an operand first acquired mid-frame sized from that frame's count;
 *                threadCleanup at release (not at a frame boundary), on another thread than the
 *                threadInit, after the last exec and before the unload; a thread index outside the
 *                slot array gets the process-wide data with a diagnostic.
 *   scanorder    ShadeopAbi_ScanOrder (FR-013, SC-007): modules in two directories define one
 *                prototype; the registry's pick and the order enumerateShadeopDeclarations visits
 *                the modules in are the same: directories in search-path order, file names
 *                lexicographic (alpha, zeta, then beta; reversed directories: beta, alpha, zeta).
 *   compiler     ShadeopAbi_CompilerAndRenderer (US4-3): oshader -I <dir> compiles probe.sl; the
 *                compiler's declaration list for the same directory names the operand; the
 *                renderer's registry resolves that prototype to a working exec.
 *
 *   Search-path directories are passed with a trailing separator, as a TSearchpath holds them. The
 *   modules are the fixtures of tests/plugins/shadeop/ and write their call log (pluginLog.h) to
 *   OR_TEST_PLUGIN_LOG.
 *
 *   argv: <mode> <plugins root (build/tests/plugins)> [<oshader> <probe.sl> <work dir>].
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "common/diagnostics.h"
#include "shadeop/shadeopDeclaration.h"
#include "shadeop/shadeopModule.h"

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
            std::vector<SReport> versionReports() const {
                std::vector<SReport> v;
                for (const SReport &r : reports)
                    if (r.code == CODE_VERSION)
                        v.push_back(r);
                return v;
            }
            std::vector<SReport> reports;
    };

    CRecordingSink g_sink;
    int g_failed = 0;
    std::string g_root;
    std::string g_log;

    void expect(bool condition, const std::string &what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what.c_str());
        }
    }

    bool contains(const std::string &text, const std::string &part) {
        return text.find(part) != std::string::npos;
    }

    // A directory of the plugins root as a search-path entry (trailing separator).
    std::string dirOf(const std::string &relative) {
        return g_root + "/" + relative + "/";
    }

    // The caller's search path: SShadeopSearchPath views the strings, which must outlive the call.
    class CPath {
        public:
            CPath(std::initializer_list<std::string> directories) : strings(directories) {
                for (const std::string &s : strings)
                    pointers.push_back(s.c_str());
            }
            CPath(const CPath &) = delete; // the pointers view this object's strings
            SShadeopSearchPath view() const {
                return SShadeopSearchPath{pointers.data(), (int)pointers.size()};
            }

        private:
            std::vector<std::string> strings;
            std::vector<const char *> pointers;
    };

    // ---- the call log of the fixtures ----
    std::vector<std::string> readLog() {
        std::vector<std::string> lines;
        std::ifstream in(g_log);
        for (std::string line; std::getline(in, line);)
            lines.push_back(line);
        return lines;
    }

    void resetCase() {
        unlink(g_log.c_str());
        g_sink.reports.clear();
    }

    // "<seq> <tag>:<call> ..." -> the lines whose call starts with `call` for the tag.
    std::vector<std::string> linesOf(const std::vector<std::string> &log, const std::string &tag, const std::string &call) {
        std::vector<std::string> v;
        for (const std::string &line : log) {
            const size_t at = line.find(' ');
            if (at != std::string::npos && line.compare(at + 1, tag.size() + 1 + call.size(), tag + ":" + call) == 0)
                v.push_back(line);
        }
        return v;
    }

    // The value after "key=" up to the next blank.
    std::string valueOf(const std::string &line, const std::string &key) {
        const size_t at = line.find(" " + key + "=");
        if (at == std::string::npos)
            return "";
        const size_t start = at + key.size() + 2;
        return line.substr(start, line.find(' ', start) - start);
    }

    // The thread id: the last field of a line.
    std::string threadOf(const std::string &line) {
        return line.substr(line.rfind(' ') + 1);
    }

    // Position of the first / last line that starts with tag:call, or -1.
    int firstIndex(const std::vector<std::string> &log, const std::string &tag, const std::string &call) {
        for (size_t i = 0; i < log.size(); ++i) {
            const size_t at = log[i].find(' ');
            if (at != std::string::npos && log[i].compare(at + 1, tag.size() + 1 + call.size(), tag + ":" + call) == 0)
                return (int)i;
        }
        return -1;
    }
    int lastIndex(const std::vector<std::string> &log, const std::string &tag, const std::string &call) {
        for (int i = (int)log.size() - 1; i >= 0; --i) {
            const size_t at = log[i].find(' ');
            if (at != std::string::npos && log[i].compare(at + 1, tag.size() + 1 + call.size(), tag + ":" + call) == 0)
                return i;
        }
        return -1;
    }

    bool entered(const std::vector<std::string> &log, const std::string &tag) {
        for (const std::string &line : log) {
            const size_t at = line.find(' ');
            if (at != std::string::npos && line.compare(at + 1, tag.size() + 1, tag + ":") == 0 && line.compare(at + 1, tag.size() + 7, tag + ":unload") != 0)
                return true;
        }
        return false;
    }

    // ---- calling an operand ----
    // probe_add(float, float): exec(data, 2, {result, a, b}).
    float callAdd(CShadeopOperand *op, void *data, float a, float b) {
        float r = -1.0f;
        void *argv[3] = {&r, &a, &b};
        shadeopOperandExec(op)(data, 2, argv);
        return r;
    }

    float callOrder(CShadeopOperand *op) {
        float r = -1.0f;
        float a = 0.0f;
        void *argv[2] = {&r, &a};
        shadeopOperandExec(op)(shadeopOperandData(op, 0), 1, argv);
        return r;
    }

    unsigned long long hexOf(const std::string &s) {
        return strtoull(s.c_str(), nullptr, 16);
    }
    unsigned long long hexOf(const void *p) {
        return (unsigned long long)(size_t)p;
    }

    // Runs `body(i)` on n threads that start together.
    void runThreads(int n, const std::function<void(int)> &body) {
        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        std::vector<std::thread> threads;
        for (int i = 0; i < n; ++i)
            threads.emplace_back([&, i] {
                ++ready;
                while (!go.load())
                    std::this_thread::yield();
                body(i);
            });
        while (ready.load() < n)
            std::this_thread::yield();
        go.store(true);
        for (std::thread &t : threads)
            t.join();
    }

    // ---- versions ----
    void versions() {
        const char *prototype = "f=ff";

        for (const char *variant : {"good", "hidden"}) {
            resetCase();
            {
                CShadeopRegistry registry;
                registry.beginFrame(4);
                const CPath path{dirOf(std::string("shadeop/") + variant)};
                CShadeopOperand *op = registry.acquire(path.view(), "probe_add", prototype);
                expect(op != nullptr, std::string(variant) + ": the operand is found");
                expect(g_sink.versionReports().empty(), std::string(variant) + ": no version diagnostic");
                if (op != nullptr)
                    expect(callAdd(op, shadeopOperandData(op, 0), 0.25f, 0.5f) == 0.75f, std::string(variant) + ": exec computes the sum");
            }
            const auto log = readLog();
            expect(firstIndex(log, variant, "init") >= 0 && firstIndex(log, variant, "exec") >= 0, std::string(variant) + ": init and exec ran");
            expect(firstIndex(log, variant, "cleanup") >= 0 && firstIndex(log, variant, "unload") > firstIndex(log, variant, "cleanup"),
                   std::string(variant) + ": cleanup, then the unload, when the registry is released");
        }

        // newer 1.9.0: one warning, still loaded
        {
            resetCase();
            CShadeopRegistry registry;
            registry.beginFrame(4);
            const CPath path{dirOf("shadeop/newer")};
            CShadeopOperand *op = registry.acquire(path.view(), "probe_add", prototype);
            expect(op != nullptr, "newer: a newer MINOR is still loaded");
            const auto reports = g_sink.versionReports();
            expect(reports.size() == 1 && reports[0].severity == DIAGNOSTIC_WARNING, "newer: exactly one CODE_VERSION warning");
            if (!reports.empty())
                expect(contains(reports[0].message, "newer.dsply") && contains(reports[0].message, "shadeop") && contains(reports[0].message, "1.9.0") &&
                           contains(reports[0].message, "1.1.0"),
                       "newer: the warning names the file, the interface and both versions: " + reports[0].message);
            // the same module acquired again (another registry, another operand): nothing more
            CShadeopRegistry again;
            again.beginFrame(4);
            again.acquire(path.view(), "probe_add", prototype);
            expect(g_sink.versionReports().size() == 1, "newer: a second load of the module prints nothing more");
        }

        // v2: rejected, nothing of the module is called
        {
            resetCase();
            {
                CShadeopRegistry registry;
                registry.beginFrame(4);
                const CPath path{dirOf("shadeop/v2")};
                CShadeopOperand *op = registry.acquire(path.view(), "probe_add", prototype);
                expect(op == nullptr, "v2: a MAJOR 2 module supplies no operand");
                const auto reports = g_sink.versionReports();
                expect(reports.size() == 1 && reports[0].severity == DIAGNOSTIC_ERROR, "v2: exactly one CODE_VERSION error");
                if (!reports.empty()) {
                    const std::string &m = reports[0].message;
                    expect(contains(m, "v2.dsply") && contains(m, "shadeop"), "v2: the error names the file and the interface: " + m);
                    expect(contains(m, "2.0.0") && contains(m, "1.1.0"), "v2: both versions: " + m);
                    expect(contains(m, "rebuild against the installed header"), "v2: the remedy: " + m);
                }
            }
            expect(!entered(readLog(), "v2"), "v2: no entry point of the rejected module was called (SC-001)");
        }

        // v2 searched before good: the scan skips it and goes on
        {
            resetCase();
            CShadeopRegistry registry;
            registry.beginFrame(4);
            const CPath path{dirOf("shadeop/v2"), dirOf("shadeop/good")};
            CShadeopOperand *op = registry.acquire(path.view(), "probe_add", prototype);
            expect(op != nullptr, "v2 then good: the later module supplies the operand");
            if (op != nullptr)
                expect(callAdd(op, shadeopOperandData(op, 0), 0.25f, 0.5f) == 0.75f, "v2 then good: it works");
            expect(g_sink.versionReports().size() == 1, "v2 then good: exactly one diagnostic (the rejection)");
            expect(!entered(readLog(), "v2"), "v2 then good: the rejected module was not called");
        }

        // legacy: no version symbol; one warning across the two operands of the module
        {
            resetCase();
            CShadeopRegistry registry;
            registry.beginFrame(4);
            const CPath path{dirOf("shadeop/legacy")};
            CShadeopOperand *add = registry.acquire(path.view(), "probe_add", prototype);
            CShadeopOperand *mul = registry.acquire(path.view(), "probe_mul", "f=ff");
            expect(add != nullptr && mul != nullptr, "legacy: both operands are found");
            const auto reports = g_sink.versionReports();
            expect(reports.size() == 1 && reports[0].severity == DIAGNOSTIC_WARNING, "legacy: exactly one warning across two operands (SC-002)");
            if (!reports.empty())
                expect(contains(reports[0].message, "legacy.dsply") && contains(reports[0].message, "1.0.0") && contains(reports[0].message, "1.2.0"),
                       "legacy: the warning names the file, 1.0.0 and the release 1.2.0: " + reports[0].message);
            if (add != nullptr)
                expect(callAdd(add, shadeopOperandData(add, 0), 0.25f, 0.5f) == 0.75f, "legacy: probe_add works");
            if (mul != nullptr) {
                float r = 0, a = 2, b = 0.375f;
                void *argv[3] = {&r, &a, &b};
                shadeopOperandExec(mul)(shadeopOperandData(mul, 0), 2, argv);
                expect(r == 0.75f, "legacy: probe_mul works");
            }
        }
    }

    // ---- perthread ----
    void perThread() {
        const int n = 8;
        resetCase();
        std::vector<void *> data(n, nullptr);
        std::vector<void *> again(n, nullptr);
        std::string mainThread;
        {
            CShadeopRegistry registry;
            registry.beginFrame(n);
            const CPath path{dirOf("shadeop/good")};
            CShadeopOperand *op = registry.acquire(path.view(), "probe_add", "f=ff");
            expect(op != nullptr, "the operand is found");
            if (op == nullptr)
                return;
            runThreads(n, [&](int i) {
                data[i] = shadeopOperandData(op, i);
                callAdd(op, data[i], 0.25f, 0.5f);
                again[i] = shadeopOperandData(op, i); // the same thread index: the same data
            });
            std::set<void *> distinct(data.begin(), data.end());
            expect((int)distinct.size() == n, "N threads get N distinct data pointers, got " + std::to_string(distinct.size()));
            expect(distinct.count(nullptr) == 0, "no thread gets NULL from a threadInit that returns a block");
            expect(data == again, "a thread index keeps its data from call to call");
            // not the process-wide data of init
            const auto inits = linesOf(readLog(), "good", "init data=");
            expect(inits.size() == 1, "init runs once for the module, not once per thread");
            if (inits.size() == 1)
                expect(distinct.count((void *)hexOf(valueOf(inits[0], "data"))) == 0, "no thread gets the process-wide init data");
        } // the registry is released here

        const auto log = readLog();
        const auto threadInits = linesOf(log, "good", "threadInit ");
        const auto threadCleans = linesOf(log, "good", "threadCleanup ");
        expect((int)threadInits.size() == n, "threadInit ran once per thread index, got " + std::to_string(threadInits.size()));
        expect((int)threadCleans.size() == n, "N cleanups after the release, got " + std::to_string(threadCleans.size()));
        std::set<unsigned long long> cleaned;
        for (const std::string &line : threadCleans)
            cleaned.insert(hexOf(valueOf(line, "data")));
        std::set<unsigned long long> expected;
        for (void *p : data)
            expected.insert(hexOf(p));
        expect(cleaned == expected, "threadCleanup receives exactly the pointers the threads were given");
        const int lastTc = lastIndex(log, "good", "threadCleanup ");
        const int cleanup = firstIndex(log, "good", "cleanup ");
        const int unload = firstIndex(log, "good", "unload");
        expect(lastIndex(log, "good", "exec ") < firstIndex(log, "good", "threadCleanup "), "every exec is before the first threadCleanup");
        expect(lastTc >= 0 && cleanup > lastTc && unload > cleanup, "threadCleanup, then cleanup, then the unload");
    }

    // ---- lifetime ----
    void lifetime() {
        // (a) lazily, once per thread index with concurrent first calls; (e) (f) the end of life
        {
            resetCase();
            const int n = 8;
            std::string mainThread;
            {
                CShadeopRegistry registry;
                registry.beginFrame(n);
                const CPath path{dirOf("shadeop/good")};
                CShadeopOperand *op = registry.acquire(path.view(), "probe_add", "f=ff");
                expect(op != nullptr, "lifetime: the operand is found");
                if (op == nullptr)
                    return;
                expect(linesOf(readLog(), "good", "threadInit ").empty(), "threadInit is lazy: not called by the acquisition");
                std::vector<void *> first(n, nullptr);
                runThreads(n, [&](int i) {
                    first[i] = shadeopOperandData(op, i);
                    callAdd(op, first[i], 0.25f, 0.5f);
                });
                const auto inits = linesOf(readLog(), "good", "threadInit ");
                std::set<std::string> indices;
                for (const std::string &line : inits)
                    indices.insert(valueOf(line, "idx"));
                expect(inits.size() == (size_t)n && indices.size() == (size_t)n, "concurrent first calls: threadInit ran once for each of 8 thread indices, got " + std::to_string(inits.size()));
                // a new frame: nothing is cleaned up, nothing is initialized again
                registry.beginFrame(n);
                for (int i = 0; i < n; ++i)
                    expect(shadeopOperandData(op, i) == first[i], "the data of thread " + std::to_string(i) + " survives a frame boundary");
                expect(linesOf(readLog(), "good", "threadCleanup ").empty(), "threadCleanup does not run at a frame boundary (FR-011 c)");
                expect(linesOf(readLog(), "good", "threadInit ").size() == (size_t)n, "no thread index is initialized twice");
                char id[32];
                snprintf(id, sizeof(id), "%llu", (unsigned long long)(size_t)pthread_self());
                mainThread = id;
            }
            const auto log = readLog();
            const auto inits = linesOf(log, "good", "threadInit ");
            const auto cleans = linesOf(log, "good", "threadCleanup ");
            expect(cleans.size() == inits.size(), "one threadCleanup per threadInit");
            // threadCleanup runs where the registry is released, not on the threads that initialized
            bool otherThread = !cleans.empty();
            for (const std::string &c : cleans)
                otherThread = otherThread && threadOf(c) == mainThread;
            for (const std::string &line : inits)
                otherThread = otherThread && threadOf(line) != mainThread;
            expect(otherThread, "threadCleanup ran on the releasing thread, not on the thread that ran threadInit (FR-011 d)");
            expect(lastIndex(log, "good", "exec ") < firstIndex(log, "good", "threadCleanup "), "threadCleanup is after the last exec");
            expect(lastIndex(log, "good", "threadCleanup ") < firstIndex(log, "good", "cleanup ") && firstIndex(log, "good", "cleanup ") < firstIndex(log, "good", "unload"),
                   "threadCleanup, then cleanup, then the unload");
        }

        // (b) a NULL result of threadInit is a value: passed through, and cleaned up
        {
            resetCase();
            {
                CShadeopRegistry registry;
                registry.beginFrame(8);
                const CPath path{dirOf("shadeop/threads_null")};
                CShadeopOperand *op = registry.acquire(path.view(), "probe_add", "f=ff");
                expect(op != nullptr, "null: the operand is found");
                if (op == nullptr)
                    return;
                expect(shadeopOperandData(op, 3) == nullptr, "null: a NULL result of threadInit is passed through for thread 3");
                expect(shadeopOperandData(op, 3) == nullptr, "null: and stays NULL (threadInit is not called again)");
                expect(shadeopOperandData(op, 2) != nullptr && shadeopOperandData(op, 4) != nullptr, "null: the other threads are unaffected");
            }
            const auto log = readLog();
            int inits3 = 0;
            for (const std::string &line : linesOf(log, "threads_null", "threadInit "))
                if (valueOf(line, "idx") == "3")
                    ++inits3;
            expect(inits3 == 1, "null: threadInit for thread 3 ran once, got " + std::to_string(inits3));
            expect(linesOf(log, "threads_null", "threadCleanup ").size() == linesOf(log, "threads_null", "threadInit ").size(),
                   "null: a threadCleanup for every initialized slot, a NULL one included");
        }

        // (c) the slots grow between frames, the existing data is kept, nothing shrinks
        {
            resetCase();
            CShadeopRegistry registry;
            registry.beginFrame(2);
            const CPath path{dirOf("shadeop/good")};
            CShadeopOperand *op = registry.acquire(path.view(), "probe_add", "f=ff");
            expect(op != nullptr, "growth: the operand is found");
            if (op == nullptr)
                return;
            void *d0 = shadeopOperandData(op, 0);
            void *d1 = shadeopOperandData(op, 1);
            registry.beginFrame(4);
            expect(shadeopOperandData(op, 0) == d0 && shadeopOperandData(op, 1) == d1, "growth: the data of the existing slots is kept");
            void *d2 = shadeopOperandData(op, 2);
            void *d3 = shadeopOperandData(op, 3);
            expect(d2 != nullptr && d3 != nullptr && d2 != d3, "growth: the new slots are usable");
            registry.beginFrame(2);
            expect(shadeopOperandData(op, 3) == d3, "growth: slots are never shrunk");
            expect(linesOf(readLog(), "good", "threadCleanup ").empty(), "growth: nothing is cleaned up by a frame boundary");
            expect(g_sink.reports.empty(), "growth: no diagnostic");
        }

        // (d) an operand first used mid-frame is sized from the current frame's count
        {
            resetCase();
            CShadeopRegistry registry;
            registry.beginFrame(4);
            const CPath path{dirOf("shadeop/good")};
            CShadeopOperand *op = registry.acquire(path.view(), "probe_add", "f=ff");
            expect(op != nullptr, "mid-frame: the operand is found");
            if (op == nullptr)
                return;
            expect(shadeopOperandData(op, 3) != nullptr, "mid-frame: thread 3 of a 4-thread frame has a slot");
            expect(g_sink.reports.empty(), "mid-frame: no diagnostic");
        }

        // (g) a thread index outside the slot array gets the process-wide data, with a diagnostic
        {
            resetCase();
            CShadeopRegistry registry;
            registry.beginFrame(4);
            const CPath path{dirOf("shadeop/good")};
            CShadeopOperand *op = registry.acquire(path.view(), "probe_add", "f=ff");
            expect(op != nullptr, "outside: the operand is found");
            if (op == nullptr)
                return;
            const auto inits = linesOf(readLog(), "good", "init data=");
            void *outside = shadeopOperandData(op, 99);
            expect(inits.size() == 1 && hexOf(outside) == hexOf(valueOf(inits[0], "data")), "outside: the data is the process-wide data init returned");
            expect(shadeopOperandData(op, 99) == outside, "outside: and stays so");
            expect(g_sink.reports.size() == 1, "outside: exactly one diagnostic for the operand, got " + std::to_string(g_sink.reports.size()));
            if (!g_sink.reports.empty())
                expect(contains(g_sink.reports[0].message, "thread"), "outside: the diagnostic names the thread: " + g_sink.reports[0].message);
        }
    }

    // ---- scanorder ----
    void scanOrder() {
        const std::vector<std::string> alphaFirst = {"f=f", "f=ff", "f=f", "f=fff", "f=f", "f=ffff"};
        const std::vector<std::string> betaFirst = {"f=f", "f=ffff", "f=f", "f=ff", "f=f", "f=fff"};

        for (int variant = 0; variant < 2; ++variant) {
            resetCase();
            const bool reversed = variant == 1;
            const CPath path = reversed ? CPath{dirOf("shadeop/order/second"), dirOf("shadeop/order/first")}
                                        : CPath{dirOf("shadeop/order/first"), dirOf("shadeop/order/second")};
            const std::string label = reversed ? "directories second, first" : "directories first, second";

            std::vector<std::string> seen;
            enumerateShadeopDeclarations(path.view(), "probe_order", [&](const SShadeopDeclaration &d) {
                expect(d.name == "probe_order", label + ": the declaration's name");
                seen.push_back(d.prototype);
            });
            expect(seen == (reversed ? betaFirst : alphaFirst), label + ": the compiler visits the modules in the order of FR-013");

            CShadeopRegistry registry;
            registry.beginFrame(1);
            CShadeopOperand *op = registry.acquire(path.view(), "probe_order", "f=f");
            expect(op != nullptr, label + ": the shared prototype is found");
            if (op != nullptr) {
                // alpha (1) is the first of directory first (alpha, zeta); beta (3) is alone in second
                const float expectedId = reversed ? 3.0f : 1.0f;
                const float got = callOrder(op);
                expect(got == expectedId, label + ": the renderer picks module id " + std::to_string((int)expectedId) + ", got " + std::to_string((int)got));
            }
        }
    }

    // ---- compiler ----
    void compilerAndRenderer(const std::string &oshader, const std::string &source, const std::string &work) {
        resetCase();
        const std::string dir = dirOf("shadeop/good");
        const std::string out = work + "/probe_compiled.rslo";
        unlink(out.c_str());

        const std::string command = "\"" + oshader + "\" -I \"" + dir + "\" -o \"" + out + "\" \"" + source + "\" > \"" + work + "/oshader.log\" 2>&1";
        const int status = system(command.c_str());
        expect(status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0, "oshader -I <dir> compiles probe.sl, status " + std::to_string(status));
        struct stat st;
        expect(stat(out.c_str(), &st) == 0 && st.st_size > 0, "oshader wrote the shader");

        // the compiler's view of the directory
        const CPath path{dir};
        std::vector<SShadeopDeclaration> declarations;
        enumerateShadeopDeclarations(path.view(), "probe_add", [&](const SShadeopDeclaration &d) { declarations.push_back(d); });
        expect(declarations.size() == 1, "the compiler's scan names the operand once, got " + std::to_string(declarations.size()));
        if (declarations.size() != 1)
            return;
        expect(declarations[0].name == "probe_add" && declarations[0].prototype == "f=ff", "the compiler's declaration is probe_add, f=ff: " + declarations[0].name + " " + declarations[0].prototype);

        // the renderer's view of the same directory, for the prototype the compiler reported
        CShadeopRegistry registry;
        registry.beginFrame(1);
        CShadeopOperand *op = registry.acquire(path.view(), declarations[0].name.c_str(), declarations[0].prototype.c_str());
        expect(op != nullptr, "the renderer resolves the prototype the compiler reported");
        if (op != nullptr)
            expect(callAdd(op, shadeopOperandData(op, 0), 0.25f, 0.5f) == 0.75f, "and runs the same operand");
    }

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: test_shadeop_registry versions|perthread|lifetime|scanorder|compiler <plugins root> [<oshader> <probe.sl> <work dir>]\n");
        return 2;
    }
    const std::string mode = argv[1];
    g_root = argv[2];
    g_log = "shadeop_registry_calls.log"; // in the test's working directory
    setenv("OR_TEST_PLUGIN_LOG", g_log.c_str(), 1);
    installDiagnosticSink(&g_sink);

    if (mode == "versions")
        versions();
    else if (mode == "perthread")
        perThread();
    else if (mode == "lifetime")
        lifetime();
    else if (mode == "scanorder")
        scanOrder();
    else if (mode == "compiler" && argc == 6)
        compilerAndRenderer(argv[3], argv[4], argv[5]);
    else {
        fprintf(stderr, "unknown mode or arguments: %s\n", mode.c_str());
        return 2;
    }

    installDiagnosticSink(nullptr);
    unlink(g_log.c_str());
    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
