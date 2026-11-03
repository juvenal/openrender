/**
 * tests/link/package/main.cpp
 *
 * The consumer of Link_PackageStaticConsumer (T-L4; spec 029, US1, FR-004, SC-002): the same scene and the same
 * assertions as tests/link/static_consumer.cpp, built against an installed package. Only the public headers are
 * visible here (ri.h), so the diagnostic sink of the in-tree test is not: the program installs an error handler
 * after RiBegin (RiErrorHandler) and treats a missing image as the sign of "no renderer backend registered", which
 * is what RiBegin's CODE_INCAPABLE leaves behind. argv[1] is the shader format ("rslo", or "slo" in a JIT build).
 */
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "ri.h"

namespace {
    int errors = 0;
    int incapable = 0;

    RtVoid handler(RtInt code, RtInt severity, const char *message) {
        fprintf(stderr, "  [handler] code %d severity %d: %s", (int)code, (int)severity, message);
        if (strstr(message, "no renderer context registered") != nullptr)
            ++incapable;
        if (severity >= RIE_ERROR)
            ++errors;
    }
} // namespace

int main(int argc, char **argv) {
    const std::string format = argc > 1 ? argv[1] : "rslo";
    const char *scene = "package_consumer.rib";
    const char *image = "package_consumer.tif";

    remove(image);
    FILE *f = fopen(scene, "w");
    if (f == nullptr) {
        fprintf(stderr, "package_consumer: cannot write %s\n", scene);
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

    RiBegin(RI_NULL);
    RiErrorHandler(handler);
    RiReadArchive((RtToken)scene, NULL, NULL);
    RiEnd();

    struct stat st;
    const bool haveImage = stat(image, &st) == 0 && st.st_size > 0;
    printf("package_consumer (%s): image %s, %d error(s), %d no-backend report(s)\n", format.c_str(), haveImage ? "written" : "ABSENT", errors, incapable);
    if (!haveImage)
        fprintf(stderr, "FAIL: no image: a renderer backend is not registered, or the render failed\n");
    return (incapable == 0 && errors == 0 && haveImage) ? 0 : 1;
}
