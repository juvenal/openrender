/**
 * tests/shading_system/test_backend.h
 *
 * Spec 033, Phase 2 (SC-003, contracts/shader-backend.md "Test backend"): a third shader backend written entirely in the
 * tests. Its key and extension are "tst" (a second one can be given another key); `load()` reads a one-line text file
 * "<name> <surface|light> <r> <g> <b>" and builds a description with a payload of its own; `instantiate()` makes a
 * CShaderInstance subclass of its own (not an RSL instance) whose execute() goes through the dispatcher and writes the
 * colour into Ci; `initialise()` counts calls and, with the arena check on, proves it runs inside the facade's
 * arenaBegin/arenaEnd. Nothing outside src/libshader/ and the tests was edited to make it work.
 */
#ifndef TESTS_SHADING_SYSTEM_TEST_BACKEND_H
#define TESTS_SHADING_SYSTEM_TEST_BACKEND_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "libshader/shading/shaderBackend.h"
#include "libshader/shading/shaderPayload.h"
#include "libshader/shading/shading.h"
#include "libshader/shading/shader.h"
#include "test_support.h"

namespace sstest {

    // What the backend saw, process-wide (the tests run one scenario at a time)
    struct STstLog {
            std::atomic<int> loads{0}, instantiates{0}, initialises{0}, executes{0};
            std::vector<std::string> events; // "load", "instantiate", "initialise", "getParameter(__category)", "setParameters"
            std::mutex lock;
            int loadDelayMs = 0; // makes concurrent first requests overlap
            // initialise(): the arena's top page, before and during the call (the facade must wrap the call in arenaBegin/End)
            const char *arenaDuring = nullptr;
            void event(const std::string &e) {
                std::lock_guard<std::mutex> guard(lock);
                events.push_back(e);
            }
            void reset() {
                loads = instantiates = initialises = executes = 0;
                std::lock_guard<std::mutex> guard(lock);
                events.clear();
                loadDelayMs = 0;
                arenaDuring = nullptr;
            }
    };
    inline STstLog &tstLog() {
        static STstLog log;
        return log;
    }

    class CTstPayload : public CShaderPayload {
        public:
            float colour[3] = {0, 0, 0};
            std::string category; // the default __category of a light
    };

    class CTstBackend;

    class CTstInstance : public CShaderInstance {
        public:
            CTstInstance(const CShader *d, CAttributes *a, CXform *x) : CShaderInstance(a, x), desc(d) {
                category = static_cast<const CTstPayload *>(d->payload)->category;
            }
            // CShaderInstance
            void illuminate(CShadingContext *ctx, float **locals) override { ctx->execute(this, locals); }
            void execute(CShadingContext *ctx, float **locals) override { ctx->execute(this, locals); }
            void setParameters(int n, const char **tokens, const void **values) override {
                tstLog().event("setParameters");
                for (int i = 0; i < n; i++)
                    if (strcmp(tokens[i], "__category") == 0)
                        category = *(const char *const *)values[i];
            }
            int getParameter(const char *name, void *dest, CVariable **, int *) override {
                if (strcmp(name, "__category") == 0 && !category.empty()) {
                    tstLog().event("getParameter(__category)");
                    *(const char **)dest = category.c_str();
                    return TRUE;
                }
                return FALSE;
            }
            unsigned int requiredParameters() override { return 0; }
            const char *getName() override { return desc->name; }
            float **prepare(CMemPage *&, float **, int) override { return nullptr; }
            const CShader *shaderDescription() const override { return desc; }

            const CShader *desc;
            std::string category;
    };

    class CTstBackend : public IShaderBackend {
        public:
            explicit CTstBackend(const char *k = "tst") : keyText(k) {}
            const char *key() const override { return keyText.c_str(); }
            const char *extension() const override { return keyText.c_str(); }

            CShader *load(const char *shaderName, const char *path) const override {
                if (tstLog().loadDelayMs > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(tstLog().loadDelayMs));
                std::ifstream f(path);
                std::string name, type, category;
                float r = 0, g = 0, b = 0;
                if (!(f >> name >> type >> r >> g >> b))
                    return nullptr;
                f >> category; // optional
                ++tstLog().loads;
                tstLog().event("load");
                CShader *desc = new CShader(shaderName);
                desc->type = (type == "light") ? SL_LIGHTSOURCE : SL_SURFACE;
                desc->backend = this;
                CTstPayload *payload = new CTstPayload;
                payload->colour[0] = r;
                payload->colour[1] = g;
                payload->colour[2] = b;
                payload->category = category;
                desc->payload = payload;
                return desc;
            }

            CShaderInstance *instantiate(CShader *desc, CAttributes *attributes, CXform *xform) const override {
                ++tstLog().instantiates;
                tstLog().event("instantiate");
                return new CTstInstance(desc, attributes, xform);
            }

            void initialise(CShaderInstance &) const override {
                ++tstLog().initialises;
                tstLog().event("initialise");
                // Allocate from the global arena: the facade's arenaBegin/arenaEnd must release it again
                (void)ralloc(64, globalArena);
                tstLog().arenaDuring = globalArena.top->memory;
            }

            void execute(CShadingContext &context, CShaderInstance &instance, float **) const override {
                ++tstLog().executes;
                const CTstPayload *p = static_cast<const CTstPayload *>(instance.shaderDescription()->payload);
                CShadingState *state = context.currentShadingState;
                for (int i = 0; i < state->numVertices; i++)
                    for (int k = 0; k < 3; k++)
                        state->varying[VARIABLE_CI][3 * i + k] = p->colour[k];
            }

            std::string keyText;
    };

    // A one-line shader file for the backend; returns its path
    inline std::string writeTstShader(const std::string &name, const std::string &type, float r, float g, float b, const std::string &category = "") {
        return writeTemp(name + ".tst", name + " " + type + " " + std::to_string(r) + " " + std::to_string(g) + " " + std::to_string(b) +
                                            (category.empty() ? "" : " " + category) + "\n");
    }

} // namespace sstest

#endif
