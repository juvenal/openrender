/**
 * tests/shading_parity/test_no_services.cpp
 *
 * The shading library's lookups with no renderer services installed (spec
 * 024-shading-services-routing, US3, T033, FR-006 and FR-014; contracts/
 * renderer-services.md "With no services installed").
 *
 * A shader load before the renderer has registered its services, or after
 * RiEnd(), must neither crash nor end the process. Each of the five routed
 * lookups returns its not-found value, reports one warning for its kind
 * (CODE_NOTSTARTED) however often it is called, and a light category that
 * could not be resolved is skipped instead of stored as ID 0.
 *
 * The test runs in its own process with no RiBegin() and constructs no
 * CShadingContext (a context built with no services is a separate, known
 * defect, GitHub #53).
 */

#include <atomic>
#include <cstdio>

#include "common/diagnostics.h"
#include "common/riVocabulary.h"
#include "libshader/shading/serviceLookup.h"
#include "libshader/shading/shading.h"

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (expr) {                                                            \
            ++g_passed;                                                        \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

// Counts what it is handed, and remembers whether every report was a
// CODE_NOTSTARTED warning
class CCountingSink : public CDiagnosticSink {
    public:
        std::atomic<int> count{0};
        std::atomic<int> notStartedWarnings{0};

        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            ++count;
            if (code == CODE_NOTSTARTED && severity == DIAGNOSTIC_WARNING)
                ++notStartedWarnings;
            fprintf(stderr, "  diagnostic (code %d, severity %d): %s", (int)code, (int)severity, message);
        }
};

int main() {
    EXPECT_TRUE(CShadingContext::defaultServices() == nullptr);

    CCountingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);

    // Every lookup, twice: not found each time, one warning per kind in all
    for (int pass = 0; pass < 2; pass++) {
        EXPECT_TRUE(libshader::lookupDSO("anyshadeop", "f=f") == nullptr);
        EXPECT_TRUE(libshader::lookupFilter("anyfilter") == RiGaussianFilter);
        EXPECT_TRUE(libshader::lookupStepFilter("anystepfilter") == RiGaussianStepFilter);
        EXPECT_TRUE(libshader::lookupGlobalID("anycategory") == 0);
        EXPECT_TRUE(libshader::lookupVariable("anyvariable") == nullptr);
    }
    EXPECT_TRUE(sink.count == 5);
    EXPECT_TRUE(sink.notStartedWarnings == 5);

    // A category that cannot be resolved is skipped, never stored as 0: the
    // list holds no entries and is terminated by the 0 the sentinel replaces
    int categories[8];
    for (int &c : categories)
        c = -1;
    const int numCategories = libshader::parseCategories("key,fill", categories);
    EXPECT_TRUE(numCategories == 0);
    EXPECT_TRUE(categories[0] == 0);
    // ... and it added no warning of its own: the global-ID warning was already given
    EXPECT_TRUE(sink.count == 5);

    installDiagnosticSink(previous);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
