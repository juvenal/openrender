/*
 * Spec 035, Phase 3: the delegating cldbl backend (see cldbl_backend.h).
 */
#include "cldbl_backend.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>

#include "closureBuiltins.h"
#include "closureTree.h"
#include "common/variable.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/shaderPayload.h"
#include "libshader/shading/shading.h"

namespace cldbl {

    namespace {

        // What the wrapper's description owns: the real description (and so the real format) it delegates to
        class CClPayload : public CShaderPayload {
            public:
                ~CClPayload() override { delete real; }
                CShader *real = nullptr;
                const IShaderBackend *realBackend = nullptr;
                std::string kind; // "" (not capable), "diffuse" or "constdiffuse"
        };

        // The wrapper's output oracle: the sums of the real instance's `output varying float r_*` variables over the real points
        struct SOutputs {
                std::mutex lock;
                std::map<std::string, double> sum;
                long points = 0;
        };
        SOutputs &outputs() {
            static SOutputs o;
            return o;
        }
        std::atomic<long> g_lightRuns{0}, g_lightPoints{0};

        // The first directory of `list` (a ':' separated path) that holds `file`, or ""
        std::string searchPath(const std::string &file) {
            const char *shaders = std::getenv("SHADERS");
            if (shaders == nullptr)
                return "";
            std::string rest = shaders;
            while (!rest.empty()) {
                const size_t colon = rest.find(':');
                const std::string dir = rest.substr(0, colon);
                std::ifstream probe(dir + "/" + file);
                if (!dir.empty() && probe.good())
                    return dir;
                if (colon == std::string::npos)
                    break;
                rest = rest.substr(colon + 1);
            }
            return "";
        }

        class CClInstance : public CShaderInstance {
            public:
                CClInstance(const CShader *d, CAttributes *a, CXform *x, CShaderInstance *w) : CShaderInstance(a, x), desc(d), wrapped(w) {
                    flags = w->flags; // a wrapped light: NONAMBIENT and the others
                    data = w->data;
                }
                ~CClInstance() override { wrapped->detach(); } // the real instance was attached once, by its own constructor

                void illuminate(CShadingContext *context, float **locals) override {
                    ++g_lightRuns; // one run of the wrapped light shader for the current batch
                    g_lightPoints += context->currentShadingState->numRealVertices;
                    wrapped->illuminate(context, locals);
                }
                void execute(CShadingContext *context, float **locals) override {
                    wrapped->instanceKey = instanceKey; // the real instance is the one the sampler sees while it runs
                    context->execute(this, locals);      // the dispatcher: this description's backend, which is the wrapper
                }
                void setParameters(int n, const char **tokens, const void **values) override { wrapped->setParameters(n, tokens, values); }
                int getParameter(const char *name, void *dest, CVariable **variable, int *index) override { return wrapped->getParameter(name, dest, variable, index); }
                bool getParameterShape(const char *name, EVariableType &type, int &numFloats) const override { return wrapped->getParameterShape(name, type, numFloats); }
                unsigned int requiredParameters() override { return wrapped->requiredParameters(); }
                const char *getName() override { return desc->name; }
                float **prepare(CMemPage *&memory, float **varying, int numVertices) override { return wrapped->prepare(memory, varying, numVertices); }
                const CShader *shaderDescription() const override { return desc; }

                const CShader *desc;
                CShaderInstance *wrapped;
        };

        class CClBackend final : public IShaderBackend {
            public:
                const char *key() const override { return "cldbl"; }
                const char *extension() const override { return "cldbl"; }

                CShader *load(const char *shaderName, const char *path) const override {
                    std::ifstream f(path);
                    std::string wrapped, format, kind;
                    if (!(f >> wrapped >> format))
                        return nullptr;
                    f >> kind; // optional: diffuse or constdiffuse
                    const std::string file = path;
                    const size_t slash = file.find_last_of('/');
                    std::string dir = slash == std::string::npos ? std::string(".") : file.substr(0, slash);
                    // a shipped shader (arealight, ...) is not beside the wrapper file: it is on the SHADERS path
                    if (!std::ifstream(dir + "/" + wrapped + "." + format).good()) {
                        const std::string found = searchPath(wrapped + "." + format);
                        if (!found.empty())
                            dir = found;
                    }
                    const IShaderBackend *real = nullptr;
                    if (format == "rslo")
                        real = &rsloBackend();
#ifdef OPENRENDER_HAVE_LLVM
                    else if (format == "slo")
                        real = &sloBackend();
#endif
                    if (real == nullptr)
                        return nullptr;
                    CShader *realDesc = real->load(wrapped.c_str(), (dir + "/" + wrapped + "." + format).c_str());
                    if (realDesc == nullptr)
                        return nullptr;
                    ++counters().loads;
                    CShader *desc = new CShader(shaderName);
                    desc->type = realDesc->type;
                    desc->usedParameters = realDesc->usedParameters;
                    desc->flags = realDesc->flags;
                    desc->backend = this;
                    CClPayload *payload = new CClPayload;
                    payload->real = realDesc;
                    payload->realBackend = real;
                    payload->kind = kind;
                    desc->payload = payload;
                    return desc;
                }

                CShaderInstance *instantiate(CShader *desc, CAttributes *attributes, CXform *xform) const override {
                    const CClPayload *payload = static_cast<const CClPayload *>(desc->payload);
                    CShaderInstance *real = payload->realBackend->instantiate(payload->real, attributes, xform);
                    ++counters().instances;
                    return new CClInstance(desc, attributes, xform, real);
                }

                void initialise(CShaderInstance &instance) const override {
                    CClInstance &w = static_cast<CClInstance &>(instance);
                    w.wrapped->shaderDescription()->backend->initialise(*w.wrapped);
                }

                void execute(CShadingContext &context, CShaderInstance &instance, float **locals) const override {
                    CClInstance &w = static_cast<CClInstance &>(instance);
                    ++counters().executes;
                    counters().realPoints += context.currentShadingState->numRealVertices;
                    counters().allVertices += context.currentShadingState->numVertices;
                    const IShaderBackend *real = w.wrapped->shaderDescription()->backend;
                    if (real->key()[0] == 'r')
                        ++counters().viaRslo;
                    else
                        ++counters().viaSlo;
                    real->execute(context, *w.wrapped, locals);
                    captureOutputs(context, *w.wrapped, locals);
                }

                // Capable instances are those whose file names a kind (T069)
                bool emitsClosures(const CShaderInstance &instance) const override {
                    const CClPayload *payload = static_cast<const CClPayload *>(instance.shaderDescription()->payload);
                    return !payload->kind.empty();
                }

                // The same delegated execute (so the lighting gate acts on the real builtins, and Ci and Oi are what execute() writes), then one tree
                // per real point: a pure function of the point's Cs and N and of the wrapper's kind
                void executeClosures(CShadingContext &context, CShaderInstance &instance, float **locals, openrender::shading::ClosureTreeBuilder &builder) const override {
                    using namespace openrender::shading;
                    execute(context, instance, locals);
                    const CClPayload *payload = static_cast<const CClPayload *>(instance.shaderDescription()->payload);
                    CShadingState *state = context.currentShadingState;
                    const bool constant = payload->kind == "constdiffuse";
                    for (int i = 0; i < state->numRealVertices; ++i) {
                        ClosureDiffuseBlock block = {{0.0f, 0.0f, 1.0f}};
                        if (!constant) {
                            const float *N = state->varying[VARIABLE_N] + 3 * i;
                            block.N[0] = N[0];
                            block.N[1] = N[1];
                            block.N[2] = N[2];
                        }
                        const float *Cs = state->varying[VARIABLE_CS] + 3 * i;
                        const float weight[3] = {Cs[0], Cs[1], Cs[2]};
                        builder.setRoot(i, builder.component(kClosureDiffuse, weight, &block, sizeof(block)));
                    }
                }

            private:
                // Sum the real instance's `output varying float r_*` variables over the real points of the batch
                static void captureOutputs(CShadingContext &context, CShaderInstance &real, float **locals) {
                    const int n = context.currentShadingState->numRealVertices;
                    SOutputs &o = outputs();
                    std::lock_guard<std::mutex> guard(o.lock);
                    bool any = false;
                    for (CVariable *v = real.parameters; v != nullptr; v = v->next) {
                        if (v->storage != STORAGE_MUTABLEPARAMETER || v->type != TYPE_FLOAT || v->container != CONTAINER_VARYING || std::strncmp(v->name, "r_", 2) != 0)
                            continue;
                        double total = 0;
                        for (int i = 0; i < n; ++i)
                            total += locals[v->entry][i * v->numFloats];
                        o.sum[v->name] += total;
                        any = true;
                    }
                    if (any)
                        o.points += n;
                }
        };

    } // namespace

    SCounters &counters() {
        static SCounters c;
        return c;
    }

    const IShaderBackend &backend() {
        static const CClBackend b;
        return b;
    }

    void resetOutputs() {
        SOutputs &o = outputs();
        std::lock_guard<std::mutex> guard(o.lock);
        o.sum.clear();
        o.points = 0;
        g_lightRuns = 0;
        g_lightPoints = 0;
    }

    double outputMean(const char *name) {
        SOutputs &o = outputs();
        std::lock_guard<std::mutex> guard(o.lock);
        const auto it = o.sum.find(name);
        return (it == o.sum.end() || o.points == 0) ? 0.0 : it->second / (double)o.points;
    }

    long lightRuns() { return g_lightRuns; }
    long lightPoints() { return g_lightPoints; }

    void registerBackend() { registerShaderBackend(&backend(), 20); }
    void unregisterBackend() { unregisterShaderBackend("cldbl"); }

} // namespace cldbl
