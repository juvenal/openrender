/**
 * tests/shading_contract/case_lightsource_rispec.cpp
 *
 * ShadingContract_LightsourceRispec{Interp,Jit}_ (GitHub #115).
 *
 * lightsource() against RISpec 3.2 section 15.8, with the real services, in an illuminance loop, under both backends:
 * "refers to the light being examined by the current iteration"; "if ... a parameter named paramname exists in that shader, and the
 * parameter is the same type as variable, then the value of that parameter is stored in variable and the function returns 1;
 * otherwise, variable is unchanged and the function returns 0"; a uniform parameter into a varying variable is converted, the
 * reverse case "is considered failure".
 *   Types:    two lights (cm_ls_light_a, cm_ls_light_b) with a float, a vector, a string and a matrix parameter each. Every
 *             iteration reads its own light (the JIT used to read the last light that ran), every type is answered (the JIT
 *             answered the first float of any parameter) and the result is the found flag, one lane per point.
 *   Mismatch{Float,Vector,String,Matrix}: the destination of another type, and a name the light lacks: the result is 0 and the destination is unchanged (the
 *             JIT wrote the parameter's first float, or 0; the interpreter copied a stored value into a destination of the wrong
 *             type).
 *   Output:   a light's output parameters, read from its saved state (the JIT read the default), and the varying-into-uniform
 *             refusal.
 */
#include <cstring>
#include <string>

#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

using namespace openrender::shading;

namespace {

    // Runs `surface` over `n` points with the lights named in `lights` bound in order
    SHarnessResult runWith(bool jit, const std::vector<const char *> &lights, const char *surface, int n, bool &ok) {
        SHarnessResult r;
        CRenderedSession s(jit);
        ok = true;
        for (const char *name : lights) {
            CProgrammableShaderInstance *light = s.load(name, SL_LIGHTSOURCE);
            CT_EXPECT(light != nullptr);
            if (light == nullptr) {
                ok = false;
                return r;
            }
            s.attributes->addLight(light);
        }
        CRProbeContext *ctx = s.makeContext(s.original);
        CProgrammableShaderInstance *shader = s.load(surface, SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader == nullptr) {
            ok = false;
            delete ctx;
            return r;
        }
        printf("  %s (%s)\n", surface, jit ? "slo" : "rslo");
        r = CRenderedSession::runSurface(ctx, shader, n, s.object);
        delete ctx;
        return r;
    }

} // namespace

int caseLightsourceRispec(bool jit, const char *family) {
    const int n = 3;
    bool ok = false;
    if (strcmp(family, "Types") == 0) {
        const SHarnessResult r = runWith(jit, {"cm_ls_light_a", "cm_ls_light_b"}, "cm_ls_all", n, ok);
        if (ok)
            for (int i = 0; i < n; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), 3.0f);  // k: 1 + 2, one light per iteration
                CT_EXPECT_NEAR(r.ciAt(i, 1), 11.0f); // xcomp(v): 1 + 10
                CT_EXPECT_NEAR(r.ciAt(i, 2), 5.0f);  // comp(m, 0, 0): 2 + 3
                CT_EXPECT_NEAR(r.oiAt(i, 0), 11.0f); // "one" + "two"
                CT_EXPECT_NEAR(r.oiAt(i, 1), 8.0f);  // four calls on each of two lights, every one 1
            }
    }
    else if (strncmp(family, "Mismatch", 8) == 0) {
        // Each destination type has its own shader: a destination that is overwritten with a value of the wrong type can crash the
        // read of the others (a string slot holding a float)
        const std::string kind = family + 8; // Float, Vector, String or Matrix
        const std::string shader = std::string("cm_ls_mm_") + (kind == "Float" ? "float" : kind == "Vector" ? "vector"
                                                                                       : kind == "String"   ? "string"
                                                                                                            : "matrix");
        const SHarnessResult r = runWith(jit, {"cm_ls_light_a"}, shader.c_str(), n, ok);
        // Ci.x is the destination read back: unchanged is -1 (float), -1 - 2 - 3 = -6 (vector), 1 (string equals "unset") and
        // comp(0,0) + comp(1,1) + comp(3,3) of `matrix d = 7` = 7 + 7 + 1 = 15 (matrix); Ci.y is the sum of the results
        const float unchanged = kind == "Float" ? -1.0f : kind == "Vector" ? -6.0f
                                                      : kind == "String"   ? 1.0f
                                                                           : 15.0f;
        if (ok)
            for (int i = 0; i < n; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), unchanged);
                CT_EXPECT_NEAR(r.ciAt(i, 1), 0.0f); // every call returned 0
            }
    }
    else if (strcmp(family, "Output") == 0) {
        const SHarnessResult r = runWith(jit, {"cm_ls_light_out"}, "cm_ls_output", n, ok);
        if (ok)
            for (int i = 0; i < n; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), 5.0f);  // the light's vary at this point, not its default
                CT_EXPECT_NEAR(r.ciAt(i, 1), -1.0f); // a varying parameter into a uniform variable: unchanged
                CT_EXPECT_NEAR(r.ciAt(i, 2), 9.0f);  // the uniform output parameter
                CT_EXPECT_NEAR(r.oiAt(i, 0), 1.0f);
                CT_EXPECT_NEAR(r.oiAt(i, 1), 0.0f);
                CT_EXPECT_NEAR(r.oiAt(i, 2), 1.0f);
            }
    }
    else {
        fprintf(stderr, "LightsourceRispec: unknown family %s\n", family);
        return 2;
    }
    return contract_test::finish();
}
