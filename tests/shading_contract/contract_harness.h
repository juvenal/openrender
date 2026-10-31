/**
 * tests/shading_contract/contract_harness.h
 *
 * The standalone program's machinery (spec 026, T048): a concrete CShadingContext, a shading state
 * of N points built from the services double's 29 globals, loaders for .rslo (parseShader) and .slo
 * (parseSloShader, sloLoader.h), and a runner that executes a surface instance on those points and
 * reads back Ci/Oi.
 *
 * The standalone program cannot construct a CSurface (that is Geometry), so the state's current
 * object is null and the services double is keyed on the null ObjectHandle (QE). Everything is
 * built from state_only_sources (CAttributes, CXform) and libshader_shading: no renderer.
 *
 * One harness per process: parseShader() and the JIT keep process-wide state.
 */
#ifndef TESTS_SHADING_CONTRACT_CONTRACT_HARNESS_H
#define TESTS_SHADING_CONTRACT_CONTRACT_HARNESS_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/memory.h"
#include "contract_services.h"
#include "contract_test.h"
#include "libshader/shading/shading.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/sloLoader.h"
#include "ri/geometry/object.h" // CObject's layout only (see attachFakeObject)
#include "ri/state/attributes.h"
#include "ri/state/xform.h"

// The .rslo loader of libshader_shading (rslo.y)
CShader *parseShader(const char *shaderName, const char *name);

// The cheapest concrete context: an empty rendering loop, nothing else
class CHarnessContext : public CShadingContext {
    public:
        explicit CHarnessContext(int thread) : CShadingContext(thread) {}
        void renderingLoop() override {}
};

class CHarness {
    public:
        // Installs the services double and builds a context with a shading state of `n` points (n <= 16)
        CHarness(CContractServices &services, int n = 4) : svc(services), numPoints(n) {
            if (globalArena.top == nullptr)
                globalArena.init();
            CShadingContext::setDefaultServices(&svc);
            context = new CHarnessContext(0);
            context->updateState();
            attributes = new CAttributes();
            xform = new CXform();
        }

        // The instances, attributes and transform are reference counted and left to process exit, like the
        // other shading tests do; only the context goes, and the services are uninstalled last.
        ~CHarness() {
            delete context;
            CShadingContext::setDefaultServices(nullptr);
        }

        CShadingState *state() { return context->currentShadingState; }

        // The message-passing locals a producer shader would have left (ACCESSOR_SURFACE, ...): installed
        // into the state before a run, in place of the executing shader's own locals
        void setMessageLocals(int accessor, float **locals) { messageLocals[accessor] = locals; }

        // A current object for the cases whose executor reads the object's attributes (the trace
        // builtins). The standalone program cannot construct a CSurface (that is Geometry), so this is a
        // zero-filled block, larger than any CSurface, whose CObject members attributes and xform are
        // set: no virtual function is ever called on it, and its handle is just a key for the services
        // double. Cases that must see a null object (the object coordinate system, FR-027) do not call it.
        CSurface *attachFakeObject() {
            static unsigned char block[4096] __attribute__((aligned(16)));
            memset(block, 0, sizeof(block));
            CObject *base = reinterpret_cast<CObject *>(block);
            base->attributes = attributes;
            base->xform = xform;
            fakeObject = reinterpret_cast<CSurface *>(block);
            return fakeObject;
        }

        // Load the compiled shader <dir>/<name>.rslo or .slo and instantiate it
        CProgrammableShaderInstance *load(const std::string &name, bool jit) {
            const std::string path = contract_test::shaderDir() + "/" + name + (jit ? ".slo" : ".rslo");
            CShader *shader = nullptr;
            if (jit) {
#ifdef OPENRENDER_HAVE_LLVM
                shader = parseSloShader(name.c_str(), path.c_str());
#endif
            }
            else {
                shader = parseShader(name.c_str(), path.c_str());
            }
            if (shader == nullptr) {
                fprintf(stderr, "harness: cannot load %s\n", path.c_str());
                return nullptr;
            }
            shaders.push_back(shader);
            CProgrammableShaderInstance *i = new CProgrammableShaderInstance(shader, attributes, xform);
            instances.push_back(i);
            return i;
        }

        // Fill the state with `numPoints` points: P, N, I, s, t, time, and a sentinel Ci/Oi
        void setupState() {
            CShadingState *ss = state();
            ss->numVertices = numPoints;
            ss->numRealVertices = numPoints;
            ss->shadingDim = SHADING_0D;
            ss->lights = nullptr;
            ss->freeLights = nullptr;
            ss->alights = nullptr;
            ss->currentLight = nullptr;
            ss->lightsExecuted = FALSE;
            ss->currentObject = fakeObject; // null unless attachFakeObject() was called (QE)
            for (int i = 0; i < numPoints; i++) {
                const float p[3] = {0.1f * (i + 1), 0.2f * (i + 1), 0.3f * (i + 1)};
                const float n[3] = {0, 0, 1};
                memcpy(ss->varying[VARIABLE_P] + 3 * i, p, sizeof(p));
                memcpy(ss->varying[VARIABLE_PS] + 3 * i, p, sizeof(p));
                memcpy(ss->varying[VARIABLE_N] + 3 * i, n, sizeof(n));
                memcpy(ss->varying[VARIABLE_NG] + 3 * i, n, sizeof(n));
                initv(ss->varying[VARIABLE_I] + 3 * i, 0, 0, 1);
                initv(ss->varying[VARIABLE_L] + 3 * i, 0, 0, 0);
                initv(ss->varying[VARIABLE_CI] + 3 * i, -1, -1, -1);
                initv(ss->varying[VARIABLE_OI] + 3 * i, -1, -1, -1);
                ss->varying[VARIABLE_S][i] = 0.1f * (i + 1);
                ss->varying[VARIABLE_T][i] = 0.2f * (i + 1);
                ss->varying[VARIABLE_DU][i] = 0;
                ss->varying[VARIABLE_DV][i] = 0;
                ss->varying[VARIABLE_TIME][i] = 0;
                ss->tags[i] = 0;
            }
            ss->numActive = numPoints;
            ss->numPassive = 0;
        }

        // Execute a surface instance on the points and read Ci/Oi back
        SHarnessResult runSurface(CProgrammableShaderInstance *surface) {
            setupState();
            CShadingState *ss = state();
            ss->currentShaderInstance = surface;

            CMemPage *localMemory = NULL;
            memoryInit(localMemory);
            float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);
            for (int a = 0; a < NUM_ACCESSORS; a++)
                if (messageLocals[a] != nullptr)
                    ss->locals[a] = messageLocals[a];
            context->execute(surface, locals);

            SHarnessResult r;
            for (int i = 0; i < numPoints; i++) {
                r.ci.insert(r.ci.end(), ss->varying[VARIABLE_CI] + 3 * i, ss->varying[VARIABLE_CI] + 3 * i + 3);
                r.oi.insert(r.oi.end(), ss->varying[VARIABLE_OI] + 3 * i, ss->varying[VARIABLE_OI] + 3 * i + 3);
            }

            ss->lights = nullptr;
            ss->freeLights = nullptr;
            for (int a = 0; a < NUM_ACCESSORS; a++)
                ss->locals[a] = nullptr;
            ss->currentShaderInstance = nullptr;
            memoryTini(localMemory);
            return r;
        }

        CContractServices &svc;
        int numPoints;
        CSurface *fakeObject = nullptr;
        float **messageLocals[NUM_ACCESSORS] = {};
        CHarnessContext *context = nullptr;
        CAttributes *attributes = nullptr;
        CXform *xform = nullptr;
        std::vector<CShader *> shaders;
        std::vector<CProgrammableShaderInstance *> instances;
};

#endif
