/*
 * Spec 035 (closure tree and BSDF): the support header of the tests in tests/closures and of Spectrum_Basics.
 *
 * A tiny check macro set (a failed check prints the expression and the values and the test goes on, so one run shows every
 * failure), a deterministic keyed random source over common/sampling.h with TEST-LOCAL keys (035 defines no call site in
 * sampling.h: FR-008), a float comparison helper that prints both values, and a diagnostic sink that records what the code under
 * test reports through common/diagnostics.h.
 */
#ifndef CLOSURES_TEST_SUPPORT_H
#define CLOSURES_TEST_SUPPORT_H

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "common/sampling.h"

namespace cltest {

inline int &failureCount() {
    static int n = 0;
    return n;
}

inline void fail(const char *file, int line, const char *what) {
    std::printf("FAIL %s:%d: %s\n", file, line, what);
    ++failureCount();
}

inline uint32_t bitsOf(float a) {
    uint32_t x;
    std::memcpy(&x, &a, 4);
    return x;
}

inline bool bitsEqual(float a, float b) { return bitsOf(a) == bitsOf(b); }

// Compares two floats for exact bit equality and prints both (and their bits) when they differ
inline bool checkBits(const char *file, int line, const char *expr, float a, float b) {
    if (bitsEqual(a, b))
        return true;
    char text[256];
    std::snprintf(text, sizeof(text), "%s: %.9g (0x%08x) != %.9g (0x%08x)", expr, (double)a, bitsOf(a), (double)b, bitsOf(b));
    fail(file, line, text);
    return false;
}

// Compares two floats within an absolute tolerance and prints both when they differ
inline bool checkNear(const char *file, int line, const char *expr, double a, double b, double tolerance) {
    if (std::fabs(a - b) <= tolerance)
        return true;
    char text[256];
    std::snprintf(text, sizeof(text), "%s: %.9g != %.9g (tolerance %.3g)", expr, a, b, tolerance);
    fail(file, line, text);
    return false;
}

inline bool checkInt(const char *file, int line, const char *expr, long long a, long long b) {
    if (a == b)
        return true;
    char text[256];
    std::snprintf(text, sizeof(text), "%s: %lld != %lld", expr, a, b);
    fail(file, line, text);
    return false;
}

} // namespace cltest

#define CL_CHECK(cond)                                         \
    do {                                                       \
        if (!(cond))                                           \
            cltest::fail(__FILE__, __LINE__, "check: " #cond); \
    } while (0)
#define CL_CHECK_INT(a, b) cltest::checkInt(__FILE__, __LINE__, #a " == " #b, (long long)(a), (long long)(b))
#define CL_CHECK_BITS(a, b) cltest::checkBits(__FILE__, __LINE__, #a " == " #b, (a), (b))
#define CL_CHECK_NEAR(a, b, tol) cltest::checkNear(__FILE__, __LINE__, #a " ~ " #b, (double)(a), (double)(b), (double)(tol))
#define CL_RESULT(name)                                                   \
    (cltest::failureCount() == 0 ? (std::printf("%s: ok\n", name), 0)     \
                                 : (std::printf("%s: %d check(s) failed\n", name, cltest::failureCount()), 1))

namespace cltest {

// A keyed random source: the independent backend of common/sampling.h under a test-local seed. The same (seed, dim, index)
// gives the same number on any thread and in any run, which is what the tests rely on.
class KeyedRandom {
    public:
        explicit KeyedRandom(uint64_t seed, uint32_t dim = 0) : key_{seed, 0}, dim_(dim), index_(0) {}
        float next() { return openrender::sampling::get1D(openrender::sampling::Backend::Independent, key_, dim_, index_++); }
        // an integer in [0, n)
        int nextInt(int n) {
            const int v = (int)(next() * (float)n);
            return v >= n ? n - 1 : v;
        }
        float range(float lo, float hi) { return lo + (hi - lo) * next(); }

    private:
        openrender::sampling::SubstreamKey key_;
        uint32_t dim_;
        uint32_t index_;
};

// Records every diagnostic reported through common/diagnostics.h while it is installed; restores the previous sink on destruction.
class CaptureSink : public CDiagnosticSink {
    public:
        struct Record {
                EErrorCode code;
                EDiagnosticSeverity severity;
                std::string message;
        };

        CaptureSink() : previous_(installDiagnosticSink(this)) {}
        ~CaptureSink() override { installDiagnosticSink(previous_); }

        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            std::lock_guard<std::mutex> lock(mutex_);
            records_.push_back({code, severity, message});
        }

        // the number of records whose message contains `text`
        int countContaining(const char *text) const {
            std::lock_guard<std::mutex> lock(mutex_);
            int n = 0;
            for (const Record &r : records_)
                if (r.message.find(text) != std::string::npos)
                    ++n;
            return n;
        }
        int total() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return (int)records_.size();
        }
        void clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            records_.clear();
        }
        // the severity of the first record whose message contains `text`, or -1
        int severityOf(const char *text) const {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const Record &r : records_)
                if (r.message.find(text) != std::string::npos)
                    return (int)r.severity;
            return -1;
        }

    private:
        mutable std::mutex mutex_;
        std::vector<Record> records_;
        CDiagnosticSink *previous_;
};

} // namespace cltest

#endif
