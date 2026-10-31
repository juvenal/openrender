/**
 * tests/shading_contract/case_matrices.cpp
 *
 * ShadingContract_Matrices{Interp,Jit} (spec 026, T054; US4 AS1-AS3, FR-008, FR-009, SC-004).
 *
 * Every coordinate-system lookup goes through the services double's getMatrix(). cm_matrices.sl runs one
 * transform per run, chosen by its `which` parameter, on a four-point state: each named system, a user
 * system, object, shader, light, NDC, a colour space, the two-space forms, the matrix forms, and a name
 * the double does not have. For each run the case checks, under one backend:
 *   - the request: the name(s) in order, the object handle (null in the standalone program, QE), the
 *     shader handle (the executing instance), the light handle (null: no light runs) and time 0 (Q14: no
 *     call site carries a time);
 *   - the transformed value equals the value computed from the double's table, so the matrix the double
 *     returned is the one Shading used (a double that honours time is exercised by RealAnswers);
 *   - "object" with a null object: the double answers NoObject, Shading reports
 *     `Object system reference without an object` once and uses the identity matrix;
 *   - an unknown name: NotFound, Shading reports `Unknown coordinate system` once and uses the identity;
 *   - the matrix-only forms (`which` 13) make no request at all.
 * The same expectations hold under both backends, so the two backends' requests are equal pairwise.
 */
#include "cases.h"

#include <cstring>

#include "case_common.h"
#include "common/colorSpace.h"
#include "contract_harness.h"
#include "contract_services.h"

using namespace openrender::shading;

namespace {

    void identity(float *m) {
        memset(m, 0, 16 * sizeof(float));
        m[0] = m[5] = m[10] = m[15] = 1;
    }
    void translation(SMatrixEntry &e, float x, float y, float z) {
        identity(e.from);
        identity(e.to);
        e.from[12] = x;
        e.from[13] = y;
        e.from[14] = z;
        e.to[12] = -x;
        e.to[13] = -y;
        e.to[14] = -z;
    }
    void scaling(SMatrixEntry &e, float s) {
        identity(e.from);
        identity(e.to);
        e.from[0] = e.from[5] = e.from[10] = s;
        e.to[0] = e.to[5] = e.to[10] = 1.0f / s;
    }

    struct STable {
            SMatrixEntry world, camera, current, user, shader, light, ndc, rgb, hsv;
    };

    // The system a name stands for, as the double's table holds it
    const SMatrixEntry &entryOf(const STable &t, const std::string &name) {
        if (name == "world")
            return t.world;
        if (name == "camera")
            return t.camera;
        if (name == "current")
            return t.current;
        if (name == "cmuser")
            return t.user;
        if (name == "shader")
            return t.shader;
        if (name == "light")
            return t.light;
        if (name == "NDC")
            return t.ndc;
        if (name == "rgb")
            return t.rgb;
        return t.hsv;
    }

    struct SExpect {
            std::vector<std::string> names; // the requests, in order
            float value[3];                 // Ci
            bool objectError;               // `Object system reference without an object`
            bool unknownWarning;            // `Unknown coordinate system`
    };

} // namespace

int caseMatrices(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        STable t;
        translation(t.world, 1, 0, 0);
        scaling(t.user, 2);
        translation(t.shader, 0, 2, 0);
        translation(t.light, 0, 0, 3);
        scaling(t.ndc, 0.5f);
        identity(t.camera.from);
        identity(t.camera.to);
        identity(t.current.from);
        identity(t.current.to);

        auto install = [&](const char *name, ECoordinateSystem kind, const SMatrixEntry &e) {
            SMatrixEntry &dst = svc.setMatrix(name, kind);
            memcpy(dst.from, e.from, sizeof(dst.from));
            memcpy(dst.to, e.to, sizeof(dst.to));
        };
        install("world", COORDINATE_WORLD, t.world);
        install("camera", COORDINATE_CAMERA, t.camera);
        install("current", COORDINATE_CURRENT, t.current);
        install("cmuser", COORDINATE_CUSTOM, t.user);
        install("shader", COORDINATE_SHADER, t.shader);
        install("light", COORDINATE_LIGHT, t.light);
        install("NDC", COORDINATE_NDC, t.ndc);
        svc.setMatrix("rgb", COLOR_RGB);
        svc.setMatrix("hsv", COLOR_HSV);
        // "object" is not in the table: with a null object the double answers NoObject. "cmunknown" is not either.

        const float p[3] = {0.25f, 0.5f, 0.75f};
        const float v[3] = {0.5f, -0.25f, 1.0f};
        const float n[3] = {0, 1, 0};
        const float c[3] = {0.2f, 0.4f, 0.6f};

        // The oracle: what the shader must have computed from the double's table (shaderFunctions.h's macros)
        std::vector<SExpect> expect(16);
        auto point = [&](int k, const char *name, bool via) {
            (void)via;
            SExpect &e = expect[k];
            e.names = {name};
            mulmp(e.value, entryOf(t, name).to, p);
            e.objectError = e.unknownWarning = false;
        };
        point(0, "world", false);
        point(1, "camera", false);
        point(2, "current", false);
        point(3, "cmuser", false);
        point(5, "shader", false);
        point(6, "light", false);
        point(7, "NDC", false);
        // 4: "object" with a null object: identity, one error
        expect[4].names = {"object"};
        memcpy(expect[4].value, p, sizeof(p));
        expect[4].objectError = true;
        expect[4].unknownWarning = false;
        // 8: ctransform("rgb", "hsv", c)
        expect[8].names = {"rgb", "hsv"};
        {
            float tmp[3];
            convertColorFrom(tmp, c, COLOR_RGB);
            convertColorTo(expect[8].value, tmp, COLOR_HSV);
        }
        expect[8].objectError = expect[8].unknownWarning = false;
        // 9: transform("world", "cmuser", p)
        expect[9].names = {"world", "cmuser"};
        {
            float tmp[3];
            mulmp(tmp, t.world.from, p);
            mulmp(expect[9].value, t.user.to, tmp);
        }
        expect[9].objectError = expect[9].unknownWarning = false;
        // 10: vtransform("world", v)
        expect[10].names = {"world"};
        mulmv(expect[10].value, t.world.to, v);
        expect[10].objectError = expect[10].unknownWarning = false;
        // 11: vtransform("cmuser", "world", v)
        expect[11].names = {"cmuser", "world"};
        {
            float tmp[3];
            mulmv(tmp, t.user.from, v);
            mulmv(expect[11].value, t.world.to, tmp);
        }
        expect[11].objectError = expect[11].unknownWarning = false;
        // 12: ntransform("world", n)
        expect[12].names = {"world"};
        mulmn(expect[12].value, t.world.from, n);
        expect[12].objectError = expect[12].unknownWarning = false;
        // 13: transform(M, p) with M the identity: no lookup at all
        expect[13].names = {};
        memcpy(expect[13].value, p, sizeof(p));
        expect[13].objectError = expect[13].unknownWarning = false;
        // 14: transform("world", M, p): M applied after the world-from matrix
        expect[14].names = {"world"};
        mulmp(expect[14].value, t.world.from, p);
        expect[14].objectError = expect[14].unknownWarning = false;
        // 15: an unknown name: identity, one warning
        expect[15].names = {"cmunknown"};
        memcpy(expect[15].value, p, sizeof(p));
        expect[15].objectError = false;
        expect[15].unknownWarning = true;

        CHarness h(svc, 4);
        CProgrammableShaderInstance *s = h.load("cm_matrices", jit);
        CT_EXPECT(s != nullptr);
        if (s != nullptr) {
            for (int which = 0; which < 16; which++) {
                printf("  which = %d (%s)\n", which, jit ? "slo" : "rslo");
                svc.matrixCalls.clear();
                const size_t reportsBefore = sink.reports.size();

                float value = (float)which;
                const char *names[1] = {"which"};
                const void *values[1] = {&value};
                s->setParameters(1, names, values);

                const SHarnessResult r = h.runSurface(s);
                const SExpect &e = expect[which];

                // The requests: names in order; object null (QE), the executing shader, no light, time 0 (Q14)
                CT_EXPECT(svc.matrixCalls.size() == e.names.size());
                for (size_t k = 0; k < svc.matrixCalls.size() && k < e.names.size(); k++) {
                    const SMatrixCall &c2 = svc.matrixCalls[k];
                    CT_EXPECT(c2.name == e.names[k]);
                    CT_EXPECT(c2.object == nullptr);
                    CT_EXPECT(c2.shader == (const void *)s);
                    CT_EXPECT(c2.light == nullptr);
                    CT_EXPECT(c2.time == 0.0f);
                }

                // The transformed value, at every lane
                for (int i = 0; i < h.numPoints; i++)
                    for (int k = 0; k < 3; k++)
                        CT_EXPECT_NEAR(r.ciAt(i, k), e.value[k]);

                // The diagnostics Shading keeps, word for word
                int objectErrors = 0, unknown = 0;
                for (size_t k = reportsBefore; k < sink.reports.size(); k++) {
                    if (sink.reports[k].message.find("Object system reference without an object") != std::string::npos)
                        ++objectErrors;
                    if (sink.reports[k].message.find("Unknown coordinate system") != std::string::npos)
                        ++unknown;
                }
                CT_EXPECT(objectErrors == (e.objectError ? 1 : 0));
                CT_EXPECT(unknown == (e.unknownWarning ? 1 : 0));
            }
        }

        // A double that honours time answers a per-time matrix when asked at another time. The builtins
        // ask at time 0 (Q14), so the per-time entry at 0.5 is never used by them: the same run gives the same value.
        SMatrixEntry &w = svc.setMatrix("world", COORDINATE_WORLD);
        SMatrixEntry::SPair other;
        identity(other.from);
        identity(other.to);
        other.to[12] = -100;
        w.perTime[0.5f] = other;
        MatrixRequest ask{"world", ObjectHandle{}, ShaderHandle{}, ShaderHandle{}, 0.5f};
        const MatrixAnswer atHalf = svc.getMatrix(ask);
        CT_EXPECT(atHalf.status == EQueryStatus::Found);
        CT_EXPECT(atHalf.to != nullptr && atHalf.to[12] == -100.0f);
        ask.time = 0.0f;
        const MatrixAnswer atZero = svc.getMatrix(ask);
        CT_EXPECT(atZero.to != nullptr && atZero.to[12] == -1.0f);
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}
