/**
 * tests/shading_contract/contract_test.h
 *
 * The few lines every contract case shares (spec 026, Phase 2): the expectation macro, the
 * counters, the result line. A case returns the process exit code.
 */
#ifndef TESTS_SHADING_CONTRACT_CONTRACT_TEST_H
#define TESTS_SHADING_CONTRACT_CONTRACT_TEST_H

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace contract_test {

    inline int &passed() {
        static int n = 0;
        return n;
    }
    inline int &failed() {
        static int n = 0;
        return n;
    }

    // What a run of a shader on N points reads back (Ci and Oi, 3 floats per point)
    struct SHarnessResult {
            std::vector<float> ci;
            std::vector<float> oi;
            float ciAt(int point, int k) const { return ci[3 * point + k]; }
            float oiAt(int point, int k) const { return oi[3 * point + k]; }
    };

    inline bool nearlyEqual(float a, float b) { return std::fabs(a - b) <= 1e-6f * (1.0f + std::fabs(a) + std::fabs(b)); }

    inline int finish() {
        printf("\nResults: %d passed, %d failed\n", passed(), failed());
        return failed() > 0 ? 1 : 0;
    }

    // The directory of the compiled test shaders (SHADING_CONTRACT_SHADERS, set by CTest)
    inline std::string shaderDir() {
        const char *d = getenv("SHADING_CONTRACT_SHADERS");
        if (d == nullptr) {
            fprintf(stderr, "contract_test: SHADING_CONTRACT_SHADERS is not set\n");
            exit(2);
        }
        return d;
    }

} // namespace contract_test

using contract_test::SHarnessResult;

#define CT_EXPECT(expr)                                                        \
    do {                                                                       \
        if (expr) {                                                            \
            ++contract_test::passed();                                         \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++contract_test::failed();                                         \
        }                                                                      \
    } while (0)

#define CT_EXPECT_NEAR(a, b) CT_EXPECT(contract_test::nearlyEqual((a), (b)))

#endif
