/**
 * tests/shading_parity/table_services.h
 *
 * Test doubles for the global-variable table a shading state is built from
 * (GitHub #59).
 *
 * CForwardingServices forwards every method to the services it wraps, and
 * leaves every method overridable.
 *
 * CCountingTableServices forwards everything and, while armed, counts the
 * globalVariable(j) queries whose index is at or past a limit the test sets.
 *
 * CTableServices stands alone: a consistent table of the 29 built-in globals
 * (VARIABLE_P .. VARIABLE_CONSTANTWIDTH, in the order and with the types
 * CRenderer::initDeclarations() registers them) and a small grid, with every
 * other method answered by a CStubServices. CStubServices itself stays the
 * inconsistent fixture (no globals, grid size 0).
 */

#ifndef TESTS_SHADING_PARITY_TABLE_SERVICES_H
#define TESTS_SHADING_PARITY_TABLE_SERVICES_H

#include <vector>

#include "common/rendererc.h"
#include "common/variable.h"
#include "recording_services.h"

///////////////////////////////////////////////////////////////////////
// Class         : CForwardingServices
// Description   : Forwards every call to `inner`
class CForwardingServices : public CRendererServices {
    public:
        explicit CForwardingServices(CRendererServices *i) : inner(i) {}


        CRendererServices *inner; // The services that were installed before this one

        int maxGridSize() const override { return inner->maxGridSize(); }
        int numGlobalVariables() const override { return inner->numGlobalVariables(); }
        CVariable *globalVariable(int i) const override { return inner->globalVariable(i); }

        float shutterOpen() const override { return inner->shutterOpen(); }
        float shutterClose() const override { return inner->shutterClose(); }
        float invShutterTime() const override { return inner->invShutterTime(); }

        float dxdPixel() const override { return inner->dxdPixel(); }
        float imagePlane() const override { return inner->imagePlane(); }
        int projection() const override { return inner->projection(); }

        unsigned int additionalParameters() const override { return inner->additionalParameters(); }

        float clipMin() const override { return inner->clipMin(); }
        float clipMax() const override { return inner->clipMax(); }
        bool hasIlluminationHook() const override { return inner->hasIlluminationHook(); }

        const float *worldBmin() const override { return inner->worldBmin(); }
        const float *worldBmax() const override { return inner->worldBmax(); }

        CTextureSystem *textureSystem() override { return inner->textureSystem(); }
        CPhotonMap *getPhotonMap(const char *name) override { return inner->getPhotonMap(name); }
        CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) override {
            return inner->getCache(handle, mode, from, to);
        }
        CTexture3d *getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE) override {
            return inner->getTexture3d(name, write, channels, from, to, hierarchy);
        }
        int texture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size) override { return inner->texture3dResolve(t, n, names, entry, size); }
        void texture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius) override { inner->texture3dLookup(t, dest, P, N, radius); }
        void texture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius) override { inner->texture3dStore(t, data, P, N, radius); }

        int getGlobalID(const char *name) override {
            return inner->getGlobalID(name);
        }

        void setOffendingObject(CObject *obj) override { inner->setOffendingObject(obj); }

        int shootStep() const override { return inner->shootStep(); }


        void getAttribute(const openrender::shading::AttributeRequest &r, openrender::shading::AttributeValue &a) const override {
            inner->getAttribute(r, a);
        }

        openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &r) const override {
            return inner->getMessage(r);
        }

        openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &r) const override {
            return inner->getMatrix(r);
        }

        RtFilterFunc getFilter(const char *name) const override {
            return inner->getFilter(name);
        }
        RtStepFilterFunc getStepFilter(const char *name) const override {
            return inner->getStepFilter(name);
        }
        CVariable *retrieveVariable(const char *name) const override {
            return inner->retrieveVariable(name);
        }
        CDSO *getDSO(const char *name, const char *prototype) override {
            return inner->getDSO(name, prototype);
        }

};

///////////////////////////////////////////////////////////////////////
// Class         : CCountingTableServices
// Description   : Forwards everything; counts globalVariable(j >= limit) while armed
class CCountingTableServices final : public CForwardingServices {
    public:
        explicit CCountingTableServices(CRendererServices *i) : CForwardingServices(i) {}

        mutable bool armed = false;     // Count only while armed
        mutable int limit = 0;          // The first index that counts
        mutable int numAtOrPastLimit = 0;

        CVariable *globalVariable(int j) const override {
            if (armed && j >= limit)
                ++numAtOrPastLimit;
            return inner->globalVariable(j);
        }
};

///////////////////////////////////////////////////////////////////////
// Class         : CTableServices
// Description   : A consistent stand-alone table: the 29 built-in globals
class CTableServices final : public CForwardingServices {
    public:
        static constexpr int kGridSize = 16;

        CTableServices() : CForwardingServices(&stub) {
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
            for (const auto &d : decls)
                add(d[0], d[1]);
        }

        // Append a global (parsed with the same grammar the renderer uses)
        bool add(const char *name, const char *decl) {
            CVariable v = {};
            if (!parseVariable(&v, name, decl))
                return false;
            v.entry = (int)table.size();
            v.storage = STORAGE_GLOBAL;
            table.push_back(v);
            return true;
        }

        int maxGridSize() const override { return kGridSize; }
        int numGlobalVariables() const override { return (int)table.size(); }
        CVariable *globalVariable(int j) const override { return const_cast<CVariable *>(&table[j]); }

        std::vector<CVariable> table;

    private:
        CStubServices stub;
};

#endif // TESTS_SHADING_PARITY_TABLE_SERVICES_H
