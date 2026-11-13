/**
 * tests/link/static_consumer_registries.cpp
 *
 * Link_StaticConsumerRegistries (spec 030, FR-006, SC-008; research R13, maintainer answer Q7). A program linked
 * plainly against ribRender_static, libshader_shading and libRIBCommon, with no -force_load, --whole-archive,
 * --export-dynamic or per-hider -u anchor, whose registries still hold every built-in. The approved
 * Link_StaticConsumer (tests/link/static_consumer.cpp) stays byte-for-byte as it is; this sibling reuses its harness.
 *
 * It exits 0 only if
 *   - the hider registry lists exactly `raytrace reyes zbuffer photon`, in that order, with the aliases `hidden` and
 *     `stochastic` on reyes and the `stochastic` alias deprecated;
 *   - the integrator registry lists exactly `classic`;
 *   - the program renders one frame with each built-in hider under Integrator "classic": an image (a photon map for
 *     the photon hider) is written and no error is reported by the registries or the frame.
 * argv[1] is the shader format ("rslo", or "slo" in a JIT build).
 *
 * Red by build failure until Phase 9 (no registry headers, no Integrator statement); on a tree where the registries
 * exist but a built-in is dropped by a plain static link it is red at run time: the missing name is printed.
 */
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "hiderRegistry.h"
#include "ri.h"
#include "transport/integratorRegistry.h"

namespace {

    class CSink : public CDiagnosticSink {
        public:
            int incapable = 0;
            int errors = 0;
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                std::fprintf(stderr, "  diagnostic (code %d, severity %d): %s", (int)code, (int)severity, message);
                if (code == CODE_INCAPABLE)
                    ++incapable;
                if (severity >= DIAGNOSTIC_ERROR)
                    ++errors;
            }
    };

    bool exists(const std::string &path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && st.st_size > 0;
    }

} // namespace

int main(int argc, char **argv) {
    const std::string format = argc > 1 ? argv[1] : "rslo";
    int failures = 0;

    CSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);

    // 1. The registries, before any session: the call graph from the core reaches every built-in.
    const char *hiders[] = {"raytrace", "reyes", "zbuffer", "photon"};
    CHiderRegistry &registry = hiderRegistry();
    std::string listed;
    for (int i = 0; i < registry.size(); ++i)
        listed += std::string(i ? " " : "") + registry.at(i)->name;
    std::printf("static_consumer_registries: hiders: %s\n", listed.c_str());
    if (registry.size() != 4) {
        std::fprintf(stderr, "FAIL: expected 4 hiders, found %d\n", registry.size());
        ++failures;
    }
    for (int i = 0; i < 4 && i < registry.size(); ++i)
        if (std::strcmp(registry.at(i)->name, hiders[i]) != 0) {
            std::fprintf(stderr, "FAIL: hider %d is \"%s\", expected \"%s\"\n", i, registry.at(i)->name, hiders[i]);
            ++failures;
        }
    for (const char *h : hiders)
        if (registry.find(h).table == nullptr) {
            std::fprintf(stderr, "FAIL: hider \"%s\" is not registered in a plain static link\n", h);
            ++failures;
        }
    const CHiderRegistry::SLookup hidden = registry.find("hidden"), stochastic = registry.find("stochastic");
    if (hidden.table == nullptr || hidden.alias == nullptr || hidden.alias->deprecationWarning != nullptr || std::strcmp(hidden.table->name, "reyes") != 0) {
        std::fprintf(stderr, "FAIL: alias \"hidden\" is not a plain alias of reyes\n");
        ++failures;
    }
    if (stochastic.table == nullptr || stochastic.alias == nullptr || stochastic.alias->deprecationWarning == nullptr ||
        std::strcmp(stochastic.table->name, "reyes") != 0) {
        std::fprintf(stderr, "FAIL: alias \"stochastic\" is not a deprecated alias of reyes\n");
        ++failures;
    }

    CIntegratorRegistry &integrators = integratorRegistry();
    std::printf("static_consumer_registries: integrators: ");
    for (int i = 0; i < integrators.size(); ++i)
        std::printf("%s%s", i ? " " : "", integrators.at(i)->name);
    std::printf("\n");
    if (integrators.size() != 1 || std::strcmp(integrators.at(0)->name, "classic") != 0) {
        std::fprintf(stderr, "FAIL: expected exactly the integrator \"classic\"\n");
        ++failures;
    }

    // 2. One frame per built-in hider, under Integrator "classic".
    for (const char *hider : hiders) {
        const bool photon = std::strcmp(hider, "photon") == 0;
        const std::string output = std::string("static_consumer_registries_") + hider + (photon ? ".gpm" : ".tif");
        const std::string scene = std::string("static_consumer_registries_") + hider + ".rib";
        std::remove(output.c_str());
        FILE *f = std::fopen(scene.c_str(), "w");
        if (f == nullptr) {
            std::fprintf(stderr, "static_consumer_registries: cannot write %s\n", scene.c_str());
            return 2;
        }
        std::fprintf(f, "Option \"shaderformat\" \"default\" [\"%s\"]\n", format.c_str());
        std::fprintf(f, "Option \"limits\" \"numthreads\" [1]\n");
        if (photon) {
            std::fprintf(f,
                         "Hider \"photon\" \"int emit\" [1000]\n"
                         "Integrator \"classic\"\n"
                         "Format 16 12 1\n"
                         "Projection \"perspective\" \"fov\" 45\n"
                         "Translate 0 -1 5\n"
                         "WorldBegin\n"
                         "  LightSource \"pointlight\" 1 \"intensity\" [4] \"from\" [0 2.2 0.5]\n"
                         "  Attribute \"photon\" \"string globalmap\" [\"%s\"]\n"
                         "  Surface \"matte\"\n"
                         "  Polygon \"P\" [-3 0 -3  -3 0 3  3 0 3  3 0 -3]\n"
                         "  Sphere 0.6 -0.6 0.6 360\n"
                         "WorldEnd\n",
                         output.c_str());
        }
        else {
            std::fprintf(f,
                         "Hider \"%s\"\n"
                         "Integrator \"classic\"\n"
                         "Display \"%s\" \"file\" \"rgba\"\n"
                         "Format 32 32 1\n"
                         "PixelSamples 1 1\n"
                         "Projection \"perspective\" \"fov\" 30\n"
                         "Translate 0 0 4\n"
                         "WorldBegin\n"
                         "  LightSource \"distantlight\" 1\n"
                         "  Surface \"matte\"\n"
                         "  Sphere 1 -1 1 360\n"
                         "WorldEnd\n",
                         hider, output.c_str());
        }
        std::fclose(f);

        const int errorsBefore = sink.errors;
        RiBegin(RI_NULL);
        RiReadArchive((RtToken)scene.c_str(), NULL, NULL);
        RiEnd();
        const bool ok = exists(output) && sink.errors == errorsBefore;
        std::printf("static_consumer_registries (%s, %s): output %s, %d error report(s)\n", format.c_str(), hider, exists(output) ? "written" : "ABSENT",
                    sink.errors - errorsBefore);
        if (!ok) {
            std::fprintf(stderr, "FAIL: hider \"%s\" did not render cleanly\n", hider);
            ++failures;
        }
    }

    installDiagnosticSink(previous);
    if (sink.incapable != 0) {
        std::fprintf(stderr, "FAIL: RiBegin reported that no renderer context is registered\n");
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}
