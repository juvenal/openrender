/**
 * tests/shading_contract/case_real_answers.cpp
 *
 * ShadingContract_RealAnswers{Interp,Jit}_{Matrices,Attributes,Messages} (spec 026, T058; US3 AS2,
 * US2 AS3, FR-022, SC-003 real side).
 *
 * The rendered program: a real RiBegin session, a real CProbeSurface (handle identity), real bound
 * shaders, and the renderer's own services behind a CContractForwarder. Each case asserts
 *   (i)  every returned value equals an oracle computed directly from CRenderer, CAttributes and the bound
 *        instances (including found/not-found and the unknown-name diagnostics), which proves master's
 *        values are kept, and
 *   (ii) the forwarder saw the version 2 call, with the object, shader and light handles: this is what
 *        makes the case red before its phase, because until then Shading does not make the call.
 *
 *   Matrices    cm_matrices.sl, each system: the oracle is CRenderer::findCoordinateSystem() and today's
 *               object/shader/camera/current resolution; request handles are the probe object and the
 *               executing instance; an unknown name warns once and uses the identity
 *   Attributes  cm_attribute.sl, cm_option.sl, cm_rendererinfo.sl with real names: ShadingRate and
 *               identifier:name from the object's CAttributes, MaxRayDepth from CRenderer, renderer from
 *               the project name; the names the renderer lacks are not found, the destination untouched
 *   Messages    cm_surface/displacement/atmosphere/incident/opposite.sl read real producers bound to the
 *               object's attributes; the oracle is the producer's own getParameter(); and cm_lightsource.sl
 *               records the documented backend difference (Q2): the interpreter asks with the iterated
 *               light and wantVariable = true, every type; the JIT with the last light that ran, float
 *               only, wantVariable = false, the value written into `result`
 */
#include "cases.h"

#include <cstring>

#include "case_common.h"
#include "common/colorSpace.h"
#include "contract_forwarder.h"
#include "ri/state/stateHandles.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

using namespace openrender::shading;

namespace {

    const void *handleOf(const CSurface *o) { return static_cast<const CObject *>(o); }

    // ---- Matrices -----------------------------------------------------------------------------------
    // Resolve a name the way CShadingContext::findCoordinateSystem() does today
    struct SOracleSystem {
            const float *from, *to;
            ECoordinateSystem kind;
            bool found;
    };

    SOracleSystem resolve(const char *name, CRenderedSession &s, CProgrammableShaderInstance *shader) {
        SOracleSystem r{identityMatrix, identityMatrix, COORDINATE_CUSTOM, false};
        if (!CRenderer::findCoordinateSystem(name, r.from, r.to, r.kind))
            return r;
        r.found = true;
        switch (r.kind) {
            case COORDINATE_OBJECT:
                r.from = s.object->xform->from;
                r.to = s.object->xform->to;
                break;
            case COORDINATE_CAMERA:
            case COORDINATE_CURRENT:
                r.from = r.to = identityMatrix;
                break;
            case COORDINATE_SHADER:
                r.from = shader->xform->from;
                r.to = shader->xform->to;
                break;
            default:
                break; // world, NDC, raster, screen, custom, colour spaces: as found
        }
        return r;
    }

    int matrices(bool jit) {
        CRenderedSession s(jit);
        CContractForwarder fwd(s.original);
        CRecordingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        {
            CRProbeContext *ctx = s.makeContext(&fwd);
            CProgrammableShaderInstance *shader = s.load("cm_matrices", SL_SURFACE);
            CT_EXPECT(shader != nullptr);
            if (shader != nullptr) {
                const float p[3] = {0.25f, 0.5f, 0.75f};
                const float v[3] = {0.5f, -0.25f, 1.0f};
                const float n[3] = {0, 1, 0};
                const float c[3] = {0.2f, 0.4f, 0.6f};

                struct SRow {
                        int which;
                        std::vector<std::string> names;
                };
                // The light system is skipped: no light runs in this state (master asserts on it)
                const std::vector<SRow> rows = {{0, {"world"}}, {1, {"camera"}}, {2, {"current"}}, {3, {"cmuser"}}, {4, {"object"}}, {5, {"shader"}},
                                                {7, {"NDC"}}, {8, {"rgb", "hsv"}}, {9, {"world", "cmuser"}}, {10, {"world"}}, {11, {"cmuser", "world"}},
                                                {12, {"world"}}, {13, {}}, {14, {"world"}}, {15, {"cmunknown"}}};
                for (const SRow &row : rows) {
                    printf("  which = %d (%s)\n", row.which, jit ? "slo" : "rslo");
                    fwd.matrixCalls.clear();
                    const size_t reportsBefore = sink.reports.size();

                    float value = (float)row.which;
                    const char *names[1] = {"which"};
                    const void *values[1] = {&value};
                    shader->setParameters(1, names, values);
                    const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);

                    // (i) the oracle
                    float expect[3] = {0, 0, 0};
                    switch (row.which) {
                        case 0: case 1: case 2: case 3: case 4: case 5: case 7: {
                            const SOracleSystem sys = resolve(row.names[0].c_str(), s, shader);
                            mulmp(expect, sys.to, p);
                        } break;
                        case 8: {
                            float tmp[3];
                            convertColorFrom(tmp, c, COLOR_RGB);
                            convertColorTo(expect, tmp, COLOR_HSV);
                        } break;
                        case 9: {
                            float tmp[3];
                            const SOracleSystem a = resolve("world", s, shader), b = resolve("cmuser", s, shader);
                            mulmp(tmp, a.from, p);
                            mulmp(expect, b.to, tmp);
                        } break;
                        case 10: {
                            const SOracleSystem a = resolve("world", s, shader);
                            mulmv(expect, a.to, v);
                        } break;
                        case 11: {
                            float tmp[3];
                            const SOracleSystem a = resolve("cmuser", s, shader), b = resolve("world", s, shader);
                            mulmv(tmp, a.from, v);
                            mulmv(expect, b.to, tmp);
                        } break;
                        case 12: {
                            const SOracleSystem a = resolve("world", s, shader);
                            mulmn(expect, a.from, n);
                        } break;
                        case 13:
                            memcpy(expect, p, sizeof(p));
                            break;
                        case 14: {
                            const SOracleSystem a = resolve("world", s, shader);
                            mulmp(expect, a.from, p);
                        } break;
                        case 15:
                            memcpy(expect, p, sizeof(p)); // an unknown name: the identity
                            break;
                    }
                    for (int i = 0; i < 3; i++)
                        for (int k = 0; k < 3; k++)
                            CT_EXPECT_NEAR(r.ciAt(i, k), expect[k]);

                    // The unknown-name diagnostic keeps its text and its count
                    int unknown = 0;
                    for (size_t k = reportsBefore; k < sink.reports.size(); k++)
                        if (sink.reports[k].message.find("Unknown coordinate system") != std::string::npos)
                            ++unknown;
                    CT_EXPECT(unknown == (row.which == 15 ? 1 : 0));

                    // (ii) the forwarder saw the v2 call, with the handles of the object and the executing instance
                    CT_EXPECT(fwd.matrixCalls.size() == row.names.size());
                    for (size_t k = 0; k < fwd.matrixCalls.size() && k < row.names.size(); k++) {
                        const SMatrixCall &c2 = fwd.matrixCalls[k];
                        CT_EXPECT(c2.name == row.names[k]);
                        CT_EXPECT(c2.object == handleOf(s.object));
                        CT_EXPECT(c2.shader == (const void *)shader);
                        CT_EXPECT(c2.time == 0.0f);
                        CT_EXPECT(c2.answered == (row.which == 15 ? EQueryStatus::NotFound : EQueryStatus::Found));
                    }
                }
            }
            delete ctx;
        }
        installDiagnosticSink(previous);
        return contract_test::finish();
    }

    // ---- Attributes ----------------------------------------------------------------------------------
    int attributes(bool jit) {
        CRenderedSession s(jit);
        CContractForwarder fwd(s.original);
        {
            s.attributes->name = const_cast<char *>("cmprobe");
            s.attributes->shadingRate = 2.5f;
            CRProbeContext *ctx = s.makeContext(&fwd);

            struct SRow {
                    const char *shader;
                    AttributeScope scope;
                    const char *floatName, *stringName;
                    float f;        // the float the shader must read (-1: not found)
                    bool stringFound;
            };
            const SRow rows[] = {
                {"cm_attribute", AttributeScope::Attribute, "ShadingRate", "identifier:name", 2.5f, true},
                {"cm_option", AttributeScope::Option, "MaxRayDepth", "cm:string", (float)CRenderer::maxRayDepth, false},
                {"cm_rendererinfo", AttributeScope::RendererInfo, "cm:float", "renderer", -1.0f, true},
            };
            for (const SRow &row : rows) {
                printf("  %s (%s)\n", row.shader, jit ? "slo" : "rslo");
                fwd.attributeCalls.clear();
                CProgrammableShaderInstance *shader = s.load(row.shader, SL_SURFACE);
                CT_EXPECT(shader != nullptr);
                if (shader == nullptr)
                    continue;
                const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);

                // (i) the oracle: the renderer's own values; the names it lacks are not found
                const bool floatFound = (row.f != -1.0f);
                const float flags = (floatFound ? 1.f : 0.f) + (row.stringFound ? 8.f : 0.f);
                for (int i = 0; i < 3; i++) {
                    CT_EXPECT_NEAR(r.ciAt(i, 0), row.f);
                    CT_EXPECT_NEAR(r.ciAt(i, 1), -1.0f); // cm:triple: not found, untouched
                    CT_EXPECT_NEAR(r.ciAt(i, 2), 1.0f);  // cm:matrix: not found, still the identity
                    CT_EXPECT_NEAR(r.oiAt(i, 0), flags);
                    CT_EXPECT_NEAR(r.oiAt(i, 1), (row.stringFound ? 1.f : 0.f) + 2 + 4);
                    CT_EXPECT_NEAR(r.oiAt(i, 2), -1.0f);
                }

                // (ii) six requests, with the scope, the names and the object's handle (Attribute scope only)
                CT_EXPECT(fwd.attributeCalls.size() == 6);
                if (fwd.attributeCalls.size() == 6) {
                    const char *names[6] = {row.floatName, "cm:triple", "cm:matrix", row.stringName, "cm:missing", "cm:short"};
                    const QueryType types[6] = {QueryType::Float, QueryType::Triple, QueryType::Matrix, QueryType::String, QueryType::Float, QueryType::Triple};
                    for (int k = 0; k < 6; k++) {
                        const SAttributeCall &c = fwd.attributeCalls[k];
                        CT_EXPECT(c.scope == row.scope);
                        CT_EXPECT(c.name == names[k]);
                        CT_EXPECT(c.expected == types[k]);
                        if (row.scope == AttributeScope::Attribute)
                            CT_EXPECT(c.object == handleOf(s.object));
                    }
                    CT_EXPECT(fwd.attributeCalls[0].answered == (floatFound ? EQueryStatus::Found : EQueryStatus::NotFound));
                    CT_EXPECT(fwd.attributeCalls[3].answered == (row.stringFound ? EQueryStatus::Found : EQueryStatus::NotFound));
                    CT_EXPECT(fwd.attributeCalls[4].answered == EQueryStatus::NotFound);
                }
            }
            delete ctx;
        }
        return contract_test::finish();
    }

    // ---- Messages ------------------------------------------------------------------------------------
    int messages(bool jit) {
        CRenderedSession s(jit);
        CContractForwarder fwd(s.original);
        {
            CRProbeContext *ctx = s.makeContext(&fwd);

            // Real producers, bound to the object's attributes
            CProgrammableShaderInstance *pSurface = s.load("cm_producer_surface", SL_SURFACE);
            CProgrammableShaderInstance *pDisplacement = s.load("cm_producer_displacement", SL_DISPLACEMENT);
            CProgrammableShaderInstance *pAtmosphere = s.load("cm_producer_volume", SL_ATMOSPHERE);
            CProgrammableShaderInstance *pInterior = s.load("cm_producer_volume", SL_ATMOSPHERE);
            CProgrammableShaderInstance *pExterior = s.load("cm_producer_volume", SL_ATMOSPHERE);
            CProgrammableShaderInstance *pLight = s.load("cm_producer_light", SL_LIGHTSOURCE);
            CT_EXPECT(pSurface && pDisplacement && pAtmosphere && pInterior && pExterior && pLight);
            if (!(pSurface && pDisplacement && pAtmosphere && pInterior && pExterior && pLight)) {
                delete ctx;
                return contract_test::finish();
            }
            s.attributes->bindSurface(ShaderGroupRef::retain(pSurface));
            s.attributes->bindDisplacement(ShaderGroupRef::retain(pDisplacement));
            s.attributes->bindAtmosphere(ShaderGroupRef::retain(pAtmosphere));
            s.attributes->bindInterior(ShaderGroupRef::retain(pInterior));
            s.attributes->bindExterior(ShaderGroupRef::retain(pExterior));
            s.attributes->addLight(pLight);

            struct SRow {
                    const char *shader;
                    CProgrammableShaderInstance *producer;
                    MessageSource source;
                    bool wantVariable;
            };
            const SRow rows[] = {
                {"cm_surface", pSurface, MessageSource::Surface, true},
                {"cm_displacement", pDisplacement, MessageSource::Displacement, true},
                {"cm_atmosphere", pAtmosphere, MessageSource::Atmosphere, true},
                {"cm_incident", pInterior, MessageSource::Interior, false},
                {"cm_opposite", pExterior, MessageSource::Exterior, false},
            };
            for (const SRow &row : rows) {
                printf("  %s (%s)\n", row.shader, jit ? "slo" : "rslo");
                fwd.messageCalls.clear();
                CProgrammableShaderInstance *shader = s.load(row.shader, SL_SURFACE);
                CT_EXPECT(shader != nullptr);
                if (shader == nullptr)
                    continue;
                const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);

                // (i) the oracle: the producer's own getParameter()
                float f = -1, vec[3] = {-1, -2, -3}, mat[16];
                char *str = nullptr;
                const int ff = row.producer->getParameter("cmfloat", &f, nullptr, nullptr);
                const int fv = row.producer->getParameter("cmvec", vec, nullptr, nullptr);
                const int fm = row.producer->getParameter("cmmatrix", mat, nullptr, nullptr);
                const int fs = row.producer->getParameter("cmstring", &str, nullptr, nullptr);
                CT_EXPECT(ff && fv && fm && fs);
                for (int i = 0; i < 3; i++) {
                    CT_EXPECT_NEAR(r.ciAt(i, 0), f);
                    CT_EXPECT_NEAR(r.ciAt(i, 1), vec[0]);
                    CT_EXPECT_NEAR(r.ciAt(i, 2), mat[5]);
                    CT_EXPECT_NEAR(r.oiAt(i, 0), 15.0f); // four found, the missing name not
                    CT_EXPECT_NEAR(r.oiAt(i, 1), 1.0f);
                    CT_EXPECT_NEAR(r.oiAt(i, 2), -7.0f);
                }

                // (ii) five requests, with the object's handle and the source the contract names
                CT_EXPECT(fwd.messageCalls.size() == 5);
                for (const SMessageCall &c : fwd.messageCalls) {
                    CT_EXPECT(c.source == row.source);
                    CT_EXPECT(c.object == handleOf(s.object));
                    CT_EXPECT(c.light == nullptr);
                    CT_EXPECT(c.wantVariable == row.wantVariable);
                }
            }

            // ---- lightsource(): the documented backend difference (Q2, research R7) ------------------------
            {
                printf("  cm_lightsource (%s)\n", jit ? "slo" : "rslo");
                fwd.messageCalls.clear();
                CProgrammableShaderInstance *shader = s.load("cm_lightsource", SL_SURFACE);
                CT_EXPECT(shader != nullptr);
                if (shader != nullptr) {
                    const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);
                    int floatRequests = 0, vectorRequests = 0;
                    bool lightHandleSeen = false;
                    for (const SMessageCall &c : fwd.messageCalls) {
                        CT_EXPECT(c.source == MessageSource::Light);
                        if (c.name == "cmfloat")
                            ++floatRequests;
                        if (c.name == "cmvec")
                            ++vectorRequests;
                        if (c.light == (const void *)pLight)
                            lightHandleSeen = true;
                    }
                    CT_EXPECT(floatRequests >= 1);
                    CT_EXPECT(lightHandleSeen);
                    if (!jit) {
                        // The interpreter asks with the iterated light, wantVariable = true, every type
                        CT_EXPECT(vectorRequests >= 1);
                        for (const SMessageCall &c : fwd.messageCalls)
                            CT_EXPECT(c.wantVariable);
                        for (int i = 0; i < 3; i++) {
                            CT_EXPECT_NEAR(r.ciAt(i, 0), 2.5f);
                            CT_EXPECT_NEAR(r.ciAt(i, 1), 0.25f);
                            CT_EXPECT_NEAR(r.ciAt(i, 2), 3.0f); // found flags: 1 + 2
                        }
                    }
                    else {
                        // The JIT asks with the last light that ran, wantVariable = false, float only; the value
                        // is written into `result` where the interpreter returns the found flag
                        for (const SMessageCall &c : fwd.messageCalls)
                            if (c.name == "cmfloat")
                                CT_EXPECT(!c.wantVariable);
                        for (int i = 0; i < 3; i++)
                            CT_EXPECT_NEAR(r.ciAt(i, 0), 2.5f);
                    }
                }
            }
            delete ctx;
        }
        return contract_test::finish();
    }

} // namespace

int caseRealAnswers(bool jit, const char *family) {
    if (strcmp(family, "Matrices") == 0)
        return matrices(jit);
    if (strcmp(family, "Attributes") == 0)
        return attributes(jit);
    if (strcmp(family, "Messages") == 0)
        return messages(jit);
    fprintf(stderr, "RealAnswers: unknown family %s\n", family);
    return 2;
}
