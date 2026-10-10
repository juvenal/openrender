// Spec 023 (FR-008, FR-015): one diagnostic sink per process, shared by every
// openRender library loaded into it.
//
//   case 0  a capture sink installed before two RiBegin/RiEnd sessions sees
//           reports before, between and after them, and is installed again
//           after the last RiEnd (idempotent RI-sink attach/detach)
//   case 1  inside RiBegin, a handler installed with RiErrorHandler receives
//           error()/warning()/info() with the RI code and severity of today
//   case 2  the error() that libshader_shading binds to is libRIBCommon's,
//           the same function this program calls, and it reaches the sink
//   case 3  reports from 8 threads while the main thread swaps sinks: none
//           is lost, nothing crashes
//
// Built twice: test_common_crosslib (shared libRIBRender) and
// test_common_crosslib_static (libRIBRender.a linked like the package's Static
// component). The static build runs cases 0 and 2 only (CROSSLIB_STATIC).
//
// Usage: test_common_crosslib <path to libshader_shading>

#include <dlfcn.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "common/diagnostics.h"
#include "ri.h"

namespace {

int failures = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                 \
            fprintf(stderr, "\n");                        \
            failures++;                                   \
        }                                                 \
    } while (0)

class CCaptureSink : public CDiagnosticSink {
    public:
        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            count++;
            lastCode = code;
            lastSeverity = severity;
            lastMessage = message;
        }
        std::atomic<int> count{0};
        EErrorCode lastCode = CODE_NOERROR;
        EDiagnosticSeverity lastSeverity = DIAGNOSTIC_INFO;
        std::string lastMessage;
};

#ifndef CROSSLIB_STATIC
struct HandlerRecord {
        int calls = 0;
        int code = -1;
        int severity = -1;
        std::string message;
} handlerRecord;

RtVoid captureHandler(RtInt code, RtInt severity, const char *message) {
    handlerRecord.calls++;
    handlerRecord.code = code;
    handlerRecord.severity = severity;
    handlerRecord.message = message;
}
#endif

void case0SessionsRestoreSink() {
    CCaptureSink capture;
    CDiagnosticSink *previous = installDiagnosticSink(&capture);

    error(CODE_BUG, "between-0\n");
    CHECK(capture.count == 1, "case 0: report before any session reached the capture sink (%d)",
          capture.count.load());

    for (int session = 0; session < 2; session++) {
        RiBegin(RI_NULL);
        RiEnd();
        const int before = capture.count;
        error(CODE_BUG, "between-%d\n", session + 1);
        CHECK(capture.count == before + 1,
              "case 0: report after session %d reached the capture sink", session + 1);
    }

    CDiagnosticSink *restored = installDiagnosticSink(previous);
    CHECK(restored == &capture, "case 0: capture sink still installed after the last RiEnd");
}

#ifndef CROSSLIB_STATIC
void case1RiHandler() {
    RiBegin(RI_NULL);
    RiErrorHandler(captureHandler);

    struct {
            int severity;
            const char *text;
    } expected[] = {{RIE_ERROR, "m1\n"}, {RIE_WARNING, "m2\n"}, {RIE_INFO, "m3\n"}};

    handlerRecord = HandlerRecord();
    error(CODE_BADTOKEN, "m1\n");
    CHECK(handlerRecord.calls == 1 && handlerRecord.code == RIE_BADTOKEN &&
              handlerRecord.severity == expected[0].severity &&
              handlerRecord.message.find(expected[0].text) != std::string::npos,
          "case 1: error() -> handler got calls=%d code=%d severity=%d message=\"%s\"", handlerRecord.calls,
          handlerRecord.code, handlerRecord.severity, handlerRecord.message.c_str());

    handlerRecord = HandlerRecord();
    warning(CODE_BADTOKEN, "m2\n");
    CHECK(handlerRecord.calls == 1 && handlerRecord.code == RIE_BADTOKEN &&
              handlerRecord.severity == expected[1].severity &&
              handlerRecord.message.find(expected[1].text) != std::string::npos,
          "case 1: warning() -> handler got calls=%d code=%d severity=%d message=\"%s\"",
          handlerRecord.calls, handlerRecord.code, handlerRecord.severity, handlerRecord.message.c_str());

    handlerRecord = HandlerRecord();
    info(CODE_BADTOKEN, "m3\n");
    CHECK(handlerRecord.calls == 1 && handlerRecord.code == RIE_BADTOKEN &&
              handlerRecord.severity == expected[2].severity &&
              handlerRecord.message.find(expected[2].text) != std::string::npos,
          "case 1: info() -> handler got calls=%d code=%d severity=%d message=\"%s\"", handlerRecord.calls,
          handlerRecord.code, handlerRecord.severity, handlerRecord.message.c_str());

    RiEnd();
}
#endif

void case2ShadingBindsCommonError(const char *shadingPath) {
    void *handle = dlopen(shadingPath, RTLD_NOW | RTLD_LOCAL);
    CHECK(handle != nullptr, "case 2: dlopen(%s): %s", shadingPath, dlerror());
    if (handle == nullptr)
        return;

    using TError = void (*)(EErrorCode, const char *, ...);
    // dlsym on a handle searches that image and the images it depends on, so
    // this is the definition libshader_shading itself resolves error() to.
    auto shadingError = reinterpret_cast<TError>(dlsym(handle, "_Z5error10EErrorCodePKcz"));
    CHECK(shadingError != nullptr, "case 2: error() not reachable from libshader_shading: %s", dlerror());
    if (shadingError != nullptr) {
        Dl_info where;
        CHECK(dladdr(reinterpret_cast<void *>(shadingError), &where) != 0 && where.dli_fname != nullptr &&
                  strstr(where.dli_fname, "libRIBCommon") != nullptr,
              "case 2: libshader_shading's error() is defined in %s, not libRIBCommon",
              where.dli_fname ? where.dli_fname : "?");
        TError mine = &error;
        CHECK(shadingError == mine, "case 2: libshader_shading's error() is not the one this program calls");

        CCaptureSink capture;
        CDiagnosticSink *previous = installDiagnosticSink(&capture);
        shadingError(CODE_BUG, "case2\n");
        installDiagnosticSink(previous);
        CHECK(capture.count == 1 && capture.lastMessage == "case2\n" && capture.lastCode == CODE_BUG &&
                  capture.lastSeverity == DIAGNOSTIC_ERROR,
              "case 2: the capture sink did not receive the report (count=%d)", capture.count.load());
    }
    dlclose(handle);
}

#ifndef CROSSLIB_STATIC
void case3Threads() {
    CCaptureSink a, b;
    CDiagnosticSink *previous = installDiagnosticSink(&a);

    constexpr int kThreads = 8;
    constexpr int kReports = 1000;
    std::vector<std::thread> workers;
    for (int t = 0; t < kThreads; t++)
        workers.emplace_back([] {
            for (int i = 0; i < kReports; i++)
                info(CODE_BUG, "thread\n");
        });
    for (int i = 0; i < 1000; i++)
        installDiagnosticSink(i % 2 ? &a : &b);
    for (std::thread &w : workers)
        w.join();
    installDiagnosticSink(previous);

    const int total = a.count + b.count;
    CHECK(total == kThreads * kReports, "case 3: %d of %d threaded reports reached a sink", total,
          kThreads * kReports);
}
#endif

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <path to libshader_shading>\n", argv[0]);
        return 2;
    }

    case0SessionsRestoreSink();
#ifndef CROSSLIB_STATIC
    case1RiHandler();
#endif
    case2ShadingBindsCommonError(argv[1]);
#ifndef CROSSLIB_STATIC
    case3Threads();
#endif

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all cross-library diagnostic checks passed\n");
    return 0;
}
