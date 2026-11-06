/**
 * tests/link/static_consumer.cpp
 *
 * Link_StaticConsumer (T-L2; spec 029, US1 AS1, FR-004, SC-002). A program linked plainly against ribRender_static,
 * libshader_shading and libRIBCommon, with no -force_load, --whole-archive or --export-dynamic, that renders the GitHub
 * #24 scene (RiBegin, Display "file", Surface "matte", one Sphere) and asserts that a renderer backend registered:
 * "the binary exists" is not evidence. argv[1] is the shader format ("rslo", or "slo" in a JIT build).
 *
 * It exits 0 only if
 *   - RiBegin reported no CODE_INCAPABLE ("Full-render mode is not available in this build ... no renderer context
 *     registered"): a sink installed before RiBegin sees it;
 *   - no error was reported at all;
 *   - the image file exists and is not empty.
 * Red on the starting tree: the program aborts at launch, because libshader_shading's imports (the bundle vtables and
 * CPhotonMap::lookup) are bound at load and nothing in a plain static link defines them.
 */
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "common/diagnostics.h"
#include "ri.h"

namespace {

    class CSink : public CDiagnosticSink {
        public:
            int incapable = 0;
            int errors = 0;
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                fprintf(stderr, "  diagnostic (code %d, severity %d): %s", (int)code, (int)severity, message);
                if (code == CODE_INCAPABLE)
                    ++incapable;
                if (severity >= DIAGNOSTIC_ERROR)
                    ++errors;
            }
    };

} // namespace

int main(int argc, char **argv) {
    const std::string format = argc > 1 ? argv[1] : "rslo";
    const char *scene = "static_consumer.rib";
    const char *image = "static_consumer.tif";

    remove(image);
    FILE *f = fopen(scene, "w");
    if (f == nullptr) {
        fprintf(stderr, "static_consumer: cannot write %s\n", scene);
        return 2;
    }
    fprintf(f,
            "Option \"shaderformat\" \"default\" [\"%s\"]\n"
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
            format.c_str(), image);
    fclose(f);

    CSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    RiBegin(RI_NULL);
    RiReadArchive((RtToken)scene, NULL, NULL);
    RiEnd();
    installDiagnosticSink(previous);

    struct stat st;
    const bool haveImage = stat(image, &st) == 0 && st.st_size > 0;
    printf("static_consumer (%s): image %s, %d error report(s), %d no-backend report(s)\n", format.c_str(), haveImage ? "written" : "ABSENT", sink.errors, sink.incapable);
    if (sink.incapable != 0)
        fprintf(stderr, "FAIL: RiBegin reported that no renderer context is registered\n");
    if (sink.errors != 0)
        fprintf(stderr, "FAIL: errors were reported\n");
    if (!haveImage)
        fprintf(stderr, "FAIL: no image\n");
    return (sink.incapable == 0 && sink.errors == 0 && haveImage) ? 0 : 1;
}
