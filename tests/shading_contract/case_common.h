/**
 * tests/shading_contract/case_common.h
 *
 * Helpers shared by the contract cases: a diagnostic sink that records what Shading reports, and the
 * expected-request vocabulary the Interp/Jit pairs use (spec 026, Phase 2).
 */
#ifndef TESTS_SHADING_CONTRACT_CASE_COMMON_H
#define TESTS_SHADING_CONTRACT_CASE_COMMON_H

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "contract_test.h"

// Records every diagnostic: code, severity, text
class CRecordingSink : public CDiagnosticSink {
    public:
        struct SReport {
                EErrorCode code;
                EDiagnosticSeverity severity;
                std::string message;
        };
        std::vector<SReport> reports;

        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            reports.push_back({code, severity, message});
            fprintf(stderr, "  diagnostic (code %d, severity %d): %s", (int)code, (int)severity, message);
        }

        // How many reports have this code and severity, and contain `text` (empty: any text)
        int count(EErrorCode code, EDiagnosticSeverity severity, const char *text = "") const {
            int n = 0;
            for (const SReport &r : reports)
                if (r.code == code && r.severity == severity && r.message.find(text) != std::string::npos)
                    ++n;
            return n;
        }
        int countText(const char *text) const {
            int n = 0;
            for (const SReport &r : reports)
                if (r.message.find(text) != std::string::npos)
                    ++n;
            return n;
        }
};

// The cases name their backend by the suffix of the ctest name
inline bool backendIsJit(const char *caseName) {
    const size_t n = strlen(caseName);
    return n >= 3 && strcmp(caseName + n - 3, "Jit") == 0;
}

#endif
