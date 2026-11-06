/**
 * tests/shading_contract/case_type_mismatch_real.cpp
 *
 * ShadingContract_TypeMismatchReal_Attribute{Interp,Jit} (spec 026, T093; FR-026, SC-013, US3 AS4, B10).
 *
 * The same refusal as case_type_mismatch_attribute.cpp, against the real services (CRendererServicesImpl),
 * which decide the TypeMismatch from what they store. It is in an executable of its own
 * (test_shading_contract_typemismatch_attr) because it calls CShadingContext's public query wrappers with
 * the QueryType argument that this phase adds (queryAttribute/queryOption/queryRendererInfo(dest, name,
 * expected)): it does not compile before Phase 5, and that is its red evidence.
 *
 * Part 1, direct calls with a canary-padded buffer (a guard pattern before and after the destination, sized
 * exactly for the expected type):
 *   user attributes (inserted into the object's CAttributes): a matrix asked for as a vector, a string asked
 *     for as a float, a float asked for as a string are refused (0 returned, canary and destination intact);
 *     a three-float vector asked for as a matrix is smaller than the destination and is copied;
 *   attribute("ShadingRate") and attribute("identifier:name") answer as before;
 *   option("Format") into three floats (the scene_state_probe.sl form) is copied and returns 1;
 *     option("Format") asked for as a string and option("sampling:sampler") asked for as a float are refused;
 *   rendererinfo: "version" (four floats) asked for as a vector, and "renderer" (a string) asked for as a
 *     float are refused; rendererinfo("renderer") asked for as a string is copied.
 * Part 2, a shader (cm_mm_real.sl, the backend of the test) that asks the real renderer for the same kinds
 * of mismatch and for the two that fit.
 * And one warning (CODE_BADTOKEN) per builtin kind per process, three in all, however many were refused.
 */
#include <cstring>

#include "case_common.h"
#include "rendered_harness.h"
#include "typemismatch_cases.h"

using namespace openrender::shading;

namespace {

    // A destination of `n` floats with a guard pattern on both sides
    struct SCanary {
            static constexpr int kGuard = 8;
            float before[kGuard];
            float data[16];
            float after[kGuard];
            int n;

            explicit SCanary(int count, float fill = -9.0f) : n(count) {
                for (int i = 0; i < kGuard; i++)
                    before[i] = after[i] = 123.0f;
                for (int i = 0; i < 16; i++)
                    data[i] = fill;
            }
            bool guardsIntact() const {
                for (int i = 0; i < kGuard; i++)
                    if (before[i] != 123.0f || after[i] != 123.0f)
                        return false;
                for (int i = n; i < 16; i++)
                    if (data[i] != -9.0f)
                        return false;
                return true;
            }
            bool untouched(float fill = -9.0f) const {
                for (int i = 0; i < n; i++)
                    if (data[i] != fill)
                        return false;
                return true;
            }
    };

    struct SStringCanary {
            const char *before = reinterpret_cast<const char *>(0x1);
            const char *data[2] = {reinterpret_cast<const char *>(0x2), reinterpret_cast<const char *>(0x3)};
            const char *after = reinterpret_cast<const char *>(0x4);
            bool intact() const {
                return before == reinterpret_cast<const char *>(0x1) && data[0] == reinterpret_cast<const char *>(0x2) &&
                       data[1] == reinterpret_cast<const char *>(0x3) && after == reinterpret_cast<const char *>(0x4);
            }
    };

    // CUserAttributeDictionary::insert() reads `value` as strings or as floats by the declaration; GCC 13 cannot see
    // that and reports -Warray-bounds against a one-float object (the same family as D2), so the type is hidden here
    __attribute__((noinline)) void insertUserAttribute(CRenderedSession &s, const char *declaration, const void *value) {
        CVariable var;
        CT_EXPECT(parseVariable(&var, nullptr, declaration));
        s.attributes->userAttributes.insert(&var, value);
    }

    void direct(CRenderedSession &s, CShadingContext *ctx) {
        ctx->setShadedObject(s.object, openrender::shading::ObjectHandle{static_cast<const CObject *>(s.object)}, s.object->attributes);

        // ---- attribute() -----------------------------------------------------------------------------
        {
            SCanary c(3);
            CT_EXPECT(ctx->queryAttribute(c.data, "user:cmmat", QueryType::Triple) == 0); // a matrix asked for as a vector
            CT_EXPECT(c.guardsIntact() && c.untouched());
        }
        {
            SCanary c(1);
            CT_EXPECT(ctx->queryAttribute(c.data, "user:cmstr", QueryType::Float) == 0); // a string asked for as a float
            CT_EXPECT(c.guardsIntact() && c.untouched());
        }
        {
            SStringCanary c;
            CT_EXPECT(ctx->queryAttribute(c.data, "user:cmnum", QueryType::String) == 0); // a float asked for as a string
            CT_EXPECT(c.intact());
        }
        {
            SCanary c(16);
            CT_EXPECT(ctx->queryAttribute(c.data, "user:cmvec", QueryType::Matrix) == 1); // smaller than the destination: copied
            CT_EXPECT(c.guardsIntact());
            CT_EXPECT_NEAR(c.data[0], 0.25f);
            CT_EXPECT_NEAR(c.data[2], 0.75f);
            CT_EXPECT(c.data[3] == -9.0f); // the tail kept
        }
        {
            SCanary c(1);
            CT_EXPECT(ctx->queryAttribute(c.data, "ShadingRate", QueryType::Float) == 1);
            CT_EXPECT_NEAR(c.data[0], 2.5f);
            CT_EXPECT(c.guardsIntact());
        }
        {
            SCanary c(1);
            CT_EXPECT(ctx->queryAttribute(c.data, "identifier:name", QueryType::Float) == 0); // a string asked for as a float
            CT_EXPECT(c.guardsIntact() && c.untouched());
        }
        {
            SStringCanary c;
            CT_EXPECT(ctx->queryAttribute(c.data, "identifier:name", QueryType::String) == 1);
            CT_EXPECT(c.data[0] != nullptr && strcmp(c.data[0], "cmprobe") == 0);
            CT_EXPECT(c.before == reinterpret_cast<const char *>(0x1) && c.after == reinterpret_cast<const char *>(0x4));
        }

        // ---- option() --------------------------------------------------------------------------------
        {
            SCanary c(3);
            CT_EXPECT(ctx->queryOption(c.data, "Format", QueryType::Float) == 1); // option("Format", fmt[3])
            CT_EXPECT_NEAR(c.data[0], (float)CRenderer::xres);
            CT_EXPECT_NEAR(c.data[1], (float)CRenderer::yres);
            CT_EXPECT_NEAR(c.data[2], 1.0f);
            CT_EXPECT(c.guardsIntact());
        }
        {
            SStringCanary c;
            CT_EXPECT(ctx->queryOption(c.data, "Format", QueryType::String) == 0); // floats stored, a string asked for
            CT_EXPECT(c.intact());
        }
        {
            SCanary c(1);
            CT_EXPECT(ctx->queryOption(c.data, "sampling:sampler", QueryType::Float) == 0); // a string stored, a float asked for
            CT_EXPECT(c.guardsIntact() && c.untouched());
        }
        {
            SStringCanary c;
            CT_EXPECT(ctx->queryOption(c.data, "sampling:sampler", QueryType::String) == 1);
            CT_EXPECT(c.data[0] != nullptr && c.before == reinterpret_cast<const char *>(0x1));
        }

        // ---- rendererinfo() --------------------------------------------------------------------------
        {
            SCanary c(3);
            CT_EXPECT(ctx->queryRendererInfo(c.data, "version", QueryType::Triple) == 0); // four floats stored, a vector asked for
            CT_EXPECT(c.guardsIntact() && c.untouched());
        }
        {
            SCanary c(1);
            CT_EXPECT(ctx->queryRendererInfo(c.data, "renderer", QueryType::Float) == 0); // a string stored, a float asked for
            CT_EXPECT(c.guardsIntact() && c.untouched());
        }
        {
            SStringCanary c;
            CT_EXPECT(ctx->queryRendererInfo(c.data, "renderer", QueryType::String) == 1);
            CT_EXPECT(c.data[0] != nullptr && c.data[0][0] != '\0');
            CT_EXPECT(c.before == reinterpret_cast<const char *>(0x1) && c.after == reinterpret_cast<const char *>(0x4));
        }
        {
            SCanary c(4);
            CT_EXPECT(ctx->queryRendererInfo(c.data, "version", QueryType::Matrix) == 1); // four floats fit a matrix
            CT_EXPECT(c.guardsIntact());
            CT_EXPECT(c.data[3] == 0.0f);
        }
    }

    void viaShader(CRenderedSession &s, CShadingContext *ctx, bool jit) {
        printf("  cm_mm_real (%s)\n", jit ? "slo" : "rslo");
        CProgrammableShaderInstance *shader = s.load("cm_mm_real", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader == nullptr)
            return;
        const int n = 4;
        for (int pass = 0; pass < 2; pass++) {
            const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, n, s.object);
            for (int i = 0; i < n; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), 0.0f);       // the three refused calls return 0
                CT_EXPECT_NEAR(r.ciAt(i, 1), 3.0f);       // the two that fit return 1: 1 + 2
                CT_EXPECT_NEAR(r.ciAt(i, 2), 8.0f);       // all eight guards intact
                CT_EXPECT_NEAR(r.oiAt(i, 0), 7.0f);       // the refused destinations untouched (1 + 2) and the string set (4)
                CT_EXPECT_NEAR(r.oiAt(i, 1), (float)CRenderer::xres);
                CT_EXPECT_NEAR(r.oiAt(i, 2), (float)CRenderer::yres);
            }
        }
    }

} // namespace

int caseTypeMismatchRealAttribute(bool jit) {
    CRecordingSink sink;
    CDiagnosticSink *previous = nullptr;
    {
        CRenderedSession s(jit);
        // RiBegin installed the renderer's own sink: ours goes in after the session starts (case_real_answers.cpp)
        previous = installDiagnosticSink(&sink);
        s.attributes->name = const_cast<char *>("cmprobe");
        s.attributes->shadingRate = 2.5f;

        // The user attributes of the object
        const float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        insertUserAttribute(s, "matrix cmmat", matrix);
        const char *str[1] = {"hello"};
        insertUserAttribute(s, "string cmstr", str);
        const float num = 2.5f;
        insertUserAttribute(s, "float cmnum", &num);
        const float vec[3] = {0.25f, 0.5f, 0.75f};
        insertUserAttribute(s, "vector cmvec", vec);

        CRProbeContext *ctx = s.makeContext(s.original);
        direct(s, ctx);
        viaShader(s, ctx, jit);
        delete ctx;
        // the counts are read before the session ends
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 3);
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, "attribute") == 1);
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, "option") == 1);
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, "rendererinfo") == 1);
        installDiagnosticSink(previous);
    }
    return contract_test::finish();
}
