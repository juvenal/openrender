/**
 * tests/shading_contract/contract_services.h
 *
 * CContractServices: a standalone CRendererServices, version 2, with no inner implementation
 * (spec 026, FR-004; contracts/test-doubles.md). It records every call in order and answers from
 * tables the test fills. It is what the standalone program installs, so a test that uses it
 * proves the shading library runs with no renderer behind it.
 *
 * Answers:
 *   maxGridSize                      64
 *   numGlobalVariables/globalVariable the 29 built-in globals, built with parseVariable() from the
 *                                    declarations CRenderer::initDeclarations() registers (copied as text;
 *                                    ShadingContract_DoubleGlobalsMatchRenderer keeps the copy honest)
 *   timing, raster, clip, bounds,
 *   additionalParameters,
 *   hasIlluminationHook, shootStep   fields the test sets
 *   getAttribute                     a table (scope, object, name) -> (status, type, values); anything
 *                                    else NotFound; a per-test switch answers Unavailable
 *   getMessage                       a table (source, object, light, name) -> a default value, or
 *                                    (variable, globalIndex); anything else NotFound
 *   getMatrix                        a table name -> (kind, from, to), with per-time entries honoured;
 *                                    "object" with a null handle -> NoObject
 *   tracer()                         the test's tracer, or nullptr
 *   the GI/3D getters, textureSystem,
 *   the lookups, getDSO,
 *   getGlobalID                      nullptr or 0, recorded
 *
 * The standalone program has no CSurface (Geometry), so its current object is null and the tables
 * are keyed on the null ObjectHandle (QE). Handle identity is checked in the rendered program.
 */
#ifndef TESTS_SHADING_CONTRACT_CONTRACT_SERVICES_H
#define TESTS_SHADING_CONTRACT_CONTRACT_SERVICES_H

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "common/rendererc.h"
#include "common/variable.h"
#include "libshader/shading/rendererServices.h"
#include "libshader/shading/serviceTypes.h"
#include "libshader/shading/tracer.h"

static_assert(openrender::shading::kRendererServicesVersion.major == 2, "CContractServices is written against services version 2");

///////////////////////////////////////////////////////////////////////
// Recorded requests
///////////////////////////////////////////////////////////////////////
struct SAttributeCall {
        openrender::shading::AttributeScope scope;
        const void *object;
        std::string name;
        openrender::shading::QueryType expected;
        openrender::shading::EQueryStatus answered;
};

struct SMessageCall {
        openrender::shading::MessageSource source;
        const void *object;
        const void *light;
        std::string name;
        openrender::shading::QueryType expected;
        bool wantVariable;
        openrender::shading::EQueryStatus answered;
};

struct SMatrixCall {
        std::string name;
        const void *object;
        const void *shader;
        const void *light;
        float time;
        openrender::shading::EQueryStatus answered;
};

///////////////////////////////////////////////////////////////////////
// Table entries
///////////////////////////////////////////////////////////////////////
struct SAttributeEntry {
        openrender::shading::EQueryStatus status = openrender::shading::EQueryStatus::Found; // Found, or TypeMismatch
        EVariableType type = TYPE_FLOAT;
        std::vector<float> floats;          // a numeric value
        std::vector<const char *> strings;  // a string value (type TYPE_STRING)
};

struct SMessageEntry {
        openrender::shading::EQueryStatus status = openrender::shading::EQueryStatus::Found; // Found, or TypeMismatch
        std::vector<float> floats;         // the default value written into dest
        std::vector<const char *> strings; // or a string default (dest receives the first as a char *)
        CVariable *variable = nullptr;     // or the variable to read the producer's current value from
        int globalIndex = -1;
};

struct SMatrixEntry {
        ECoordinateSystem kind = COORDINATE_CUSTOM;
        float from[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        float to[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        struct SPair {
                float from[16];
                float to[16];
        };
        std::map<float, SPair> perTime; // time -> matrices, consulted first when the request's time matches
};

///////////////////////////////////////////////////////////////////////
// Class         : CContractServices
///////////////////////////////////////////////////////////////////////
class CContractServices final : public CRendererServices {
    public:
        CContractServices() {
            // The 29 built-in globals: the declarations CRenderer::initDeclarations() registers, in order
            static const char *const decls[][2] = {
                {"P", "global vertex point"}, {"Ps", "global vertex point"}, {"N", "global varying normal"},
                {"Ng", "global varying normal"}, {"dPdu", "global vertex vector"}, {"dPdv", "global vertex vector"},
                {"L", "global varying vector"}, {"Cs", "global varying color"}, {"Os", "global varying color"},
                {"Cl", "global varying color"}, {"Ol", "global varying color"}, {"Ci", "global varying color"},
                {"Oi", "global varying color"}, {"s", "global varying float"}, {"t", "global varying float"},
                {"du", "global varying float"}, {"dv", "global varying float"}, {"u", "global varying float"},
                {"v", "global varying float"}, {"I", "global varying vector"}, {"E", "global varying point"},
                {"alpha", "global varying float"}, {"time", "global varying float"}, {"Pw", "global vertex htpoint"},
                {"ncomps", "global uniform float"}, {"dtime", "global uniform float"}, {"dPdtime", "global varying vector"},
                {"width", "global varying float"}, {"constantwidth", "global constant float"},
            };
            for (const auto &d : decls) {
                CVariable v = {};
                parseVariable(&v, d[0], d[1]);
                v.entry = (int)table.size();
                v.storage = STORAGE_GLOBAL;
                table.push_back(v);
            }
        }

        // ---- what the test sets ------------------------------------------------------------------
        std::vector<CVariable> table;                        // the globals
        float shutterOpenValue = 0, shutterCloseValue = 1, invShutterValue = 1;
        float dxdPixelValue = 1, imagePlaneValue = 1;
        int projectionValue = 0;
        unsigned int additionalValue = 0;
        float clipMinValue = 0.01f, clipMaxValue = 1e30f;
        bool illuminationHookValue = false;
        float bmin[3] = {-10, -10, -10}, bmax[3] = {10, 10, 10};
        int shootStepValue = 16;
        openrender::shading::ITracer *tracerValue = nullptr; // what tracer() returns

        // Per-test switches: the service answers Unavailable
        bool attributeUnavailable = false, messageUnavailable = false, matrixUnavailable = false;

        void setAttribute(openrender::shading::AttributeScope scope, const void *object, const char *name, const SAttributeEntry &e) {
            attributes[std::make_tuple((int)scope, object, std::string(name))] = e;
        }
        void setFloat(openrender::shading::AttributeScope scope, const void *object, const char *name, float v) {
            SAttributeEntry e;
            e.type = TYPE_FLOAT;
            e.floats = {v};
            setAttribute(scope, object, name, e);
        }
        void setTriple(openrender::shading::AttributeScope scope, const void *object, const char *name, float a, float b, float c) {
            SAttributeEntry e;
            e.type = TYPE_VECTOR;
            e.floats = {a, b, c};
            setAttribute(scope, object, name, e);
        }
        void setMatrixValue(openrender::shading::AttributeScope scope, const void *object, const char *name, const float *m16) {
            SAttributeEntry e;
            e.type = TYPE_MATRIX;
            e.floats.assign(m16, m16 + 16);
            setAttribute(scope, object, name, e);
        }
        void setString(openrender::shading::AttributeScope scope, const void *object, const char *name, const char *s) {
            SAttributeEntry e;
            e.type = TYPE_STRING;
            e.strings = {s};
            setAttribute(scope, object, name, e);
        }

        void setMessage(openrender::shading::MessageSource source, const void *object, const void *light, const char *name, const SMessageEntry &e) {
            messages[std::make_tuple((int)source, object, light, std::string(name))] = e;
        }
        void setMessageFloats(openrender::shading::MessageSource source, const void *object, const void *light, const char *name, std::vector<float> values) {
            SMessageEntry e;
            e.floats = std::move(values);
            setMessage(source, object, light, name, e);
        }

        SMatrixEntry &setMatrix(const char *name, ECoordinateSystem kind) {
            SMatrixEntry &e = matrices[name];
            e.kind = kind;
            return e;
        }

        // ---- what the double saw ------------------------------------------------------------------
        mutable std::vector<std::string> calls;             // every recorded method, in order
        mutable std::vector<SAttributeCall> attributeCalls;
        mutable std::vector<SMessageCall> messageCalls;
        mutable std::vector<SMatrixCall> matrixCalls;
        mutable int globalQueries = 0;                      // numGlobalVariables() and globalVariable(i) together
        mutable int tracerCalls = 0;
        mutable int offendingObjectCalls = 0;
        mutable const void *lastOffending = nullptr;

        int count(const char *method) const {
            int n = 0;
            for (const std::string &c : calls)
                if (c == method)
                    ++n;
            return n;
        }

        // ---- CRendererServices: unchanged members ------------------------------------------------------
        int maxGridSize() const override { return 64; }
        int numGlobalVariables() const override {
            ++globalQueries;
            return (int)table.size();
        }
        CVariable *globalVariable(int i) const override {
            ++globalQueries;
            return const_cast<CVariable *>(&table[i]);
        }

        float shutterOpen() const override { return shutterOpenValue; }
        float shutterClose() const override { return shutterCloseValue; }
        float invShutterTime() const override { return invShutterValue; }

        float dxdPixel() const override { return dxdPixelValue; }
        float imagePlane() const override { return imagePlaneValue; }
        int projection() const override { return projectionValue; }

        unsigned int additionalParameters() const override { return additionalValue; }

        float clipMin() const override { return clipMinValue; }
        float clipMax() const override { return clipMaxValue; }
        bool hasIlluminationHook() const override { return illuminationHookValue; }

        const float *worldBmin() const override { return bmin; }
        const float *worldBmax() const override { return bmax; }

        CPhotonMap *getPhotonMap(const char *) override {
            calls.push_back("getPhotonMap");
            return nullptr;
        }
        CTexture3d *getCache(const char *, const char *, const float *, const float *) override {
            calls.push_back("getCache");
            return nullptr;
        }
        CTexture3d *getTexture3d(const char *, int, const char *, const float *, const float *, int = FALSE) override {
            calls.push_back("getTexture3d");
            return nullptr;
        }
        int texture3dResolve(CTexture3d *, int, const char **, int *, int *) override {
            calls.push_back("texture3dResolve");
            return 0;
        }
        void texture3dLookup(CTexture3d *, float *, const float *, const float *, float) override { calls.push_back("texture3dLookup"); }
        void texture3dStore(CTexture3d *, const float *, const float *, const float *, float) override { calls.push_back("texture3dStore"); }

        CTextureSystem *textureSystem() override {
            calls.push_back("textureSystem");
            return nullptr;
        }

        int getGlobalID(const char *) override {
            calls.push_back("getGlobalID");
            return 0;
        }

        int shootStep() const override { return shootStepValue; }

        RtFilterFunc getFilter(const char *) const override {
            calls.push_back("getFilter");
            return nullptr;
        }
        RtStepFilterFunc getStepFilter(const char *) const override {
            calls.push_back("getStepFilter");
            return nullptr;
        }
        // The shader loader resolves every global a shader names (Ci, P, ...) through this lookup, so the double
        // answers it from its own table of the 29 built-in globals; any other name is not a variable.
        CVariable *retrieveVariable(const char *name) const override {
            calls.push_back("retrieveVariable");
            for (const CVariable &v : table)
                if (strcmp(v.name, name) == 0)
                    return const_cast<CVariable *>(&v);
            return nullptr;
        }
        CDSO *getDSO(const char *, const char *) override {
            calls.push_back("getDSO");
            return nullptr;
        }

        // ---- CRendererServices: version 2 ----------------------------------------------------------------
        void getAttribute(const openrender::shading::AttributeRequest &r, openrender::shading::AttributeValue &answer) const override {
            using namespace openrender::shading;
            calls.push_back("getAttribute");
            SAttributeCall c{r.scope, r.object.ptr, r.name, r.expected, EQueryStatus::Unavailable};
            if (!attributeUnavailable) {
                // Option and RendererInfo ignore the object, as the contract says
                const void *obj = (r.scope == AttributeScope::Attribute) ? r.object.ptr : nullptr;
                auto it = attributes.find(std::make_tuple((int)r.scope, obj, std::string(r.name)));
                if (it == attributes.end()) {
                    answer.status = EQueryStatus::NotFound;
                }
                else {
                    const SAttributeEntry &e = it->second;
                    answer.status = e.status;
                    answer.type = e.type;
                    if (e.type == TYPE_STRING) {
                        answer.count = (int)e.strings.size();
                        answer.data = e.strings.data();
                    }
                    else {
                        answer.count = (int)e.floats.size();
                        answer.data = e.floats.data();
                    }
                }
                c.answered = answer.status;
            }
            attributeCalls.push_back(c);
        }

        openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &r) const override {
            using namespace openrender::shading;
            calls.push_back("getMessage");
            MessageAnswer a;
            if (!messageUnavailable) {
                auto it = messages.find(std::make_tuple((int)r.source, r.object.ptr, r.light.ptr, std::string(r.name)));
                if (it == messages.end()) {
                    a.status = EQueryStatus::NotFound;
                }
                else {
                    const SMessageEntry &e = it->second;
                    a.status = e.status;
                    if (e.status == EQueryStatus::Found) {
                        // TypeMismatch writes nothing into dest, as the contract requires
                        if (e.variable != nullptr && r.wantVariable) {
                            a.variable = e.variable;
                            a.globalIndex = e.globalIndex;
                        }
                        else if (!e.strings.empty()) {
                            *(const char **)r.dest = e.strings[0];
                        }
                        else if (!e.floats.empty()) {
                            memcpy(r.dest, e.floats.data(), e.floats.size() * sizeof(float));
                        }
                    }
                }
            }
            messageCalls.push_back({r.source, r.object.ptr, r.light.ptr, r.name, r.expected, r.wantVariable, a.status});
            return a;
        }

        openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &r) const override {
            using namespace openrender::shading;
            calls.push_back("getMatrix");
            MatrixAnswer a;
            if (!matrixUnavailable) {
                if (strcmp(r.name, "object") == 0 && !r.object) {
                    a.status = EQueryStatus::NoObject;
                }
                else {
                    auto it = matrices.find(r.name);
                    if (it == matrices.end()) {
                        a.status = EQueryStatus::NotFound;
                    }
                    else {
                        const SMatrixEntry &e = it->second;
                        a.status = EQueryStatus::Found;
                        a.kind = e.kind;
                        a.from = e.from;
                        a.to = e.to;
                        auto t = e.perTime.find(r.time);
                        if (t != e.perTime.end()) {
                            a.from = t->second.from;
                            a.to = t->second.to;
                        }
                    }
                }
            }
            matrixCalls.push_back({r.name, r.object.ptr, r.shader.ptr, r.light.ptr, r.time, a.status});
            return a;
        }

        openrender::shading::ITracer *tracer() override {
            ++tracerCalls;
            calls.push_back("tracer");
            return tracerValue;
        }

        // Error reporting (version 2: the handle of the object that caused the error)
        void setOffendingObject(openrender::shading::ObjectHandle object) override {
            ++offendingObjectCalls;
            lastOffending = object.ptr;
            calls.push_back("setOffendingObject");
        }

    private:
        std::map<std::tuple<int, const void *, std::string>, SAttributeEntry> attributes;
        std::map<std::tuple<int, const void *, const void *, std::string>, SMessageEntry> messages;
        std::map<std::string, SMatrixEntry> matrices;
};

#endif
