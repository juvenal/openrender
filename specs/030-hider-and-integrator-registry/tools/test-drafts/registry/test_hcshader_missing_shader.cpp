/**
 * Project: openRender
 *
 * File: test_hcshader_missing_shader.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US5 AS2, FR-026. Hcshader_MissingShader.
 *
 *   With the built-in sphere and quad lights retired, a light named "spherelight" or "quadlight" that
 *   no compiled shader answers for is an ordinary missing shader: the error
 *   `Failed to find shader "<name>"` (CODE_NOSHADER) and no light. On the starting tree the C++
 *   substitute (src/ri/hcshader.cpp) answers instead, so no error is reported: this test is red by
 *   assertion until Phase 4 deletes it.
 *
 *   The shader search path of the frame is an empty directory (argv[1]), so no .slo/.rslo can answer.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <string>

#include "registry_testing.h"
#include "ri.h"

using namespace regtest;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <empty shader directory>\n", argv[0]);
        return 2;
    }
    const std::string emptyDir = argv[1];

    for (const char *light : {"spherelight", "quadlight"}) {
        const std::string rib = "Option \"searchpath\" \"shader\" [\"" + emptyDir + "\"]\n"
                                "Option \"limits\" \"numthreads\" [1]\n"
                                "Format 16 16 1\n"
                                "Projection \"perspective\" \"fov\" 30\n"
                                "Translate 0 0 4\n"
                                "WorldBegin\n"
                                "  LightSource \"" + std::string(light) + "\" 1\n"
                                "  Sphere 1 -1 1 360\n"
                                "WorldEnd\n";
        const SSession s = renderRib(rib, std::string("hcshader_missing_") + light + ".rib");
        const std::string expected = std::string("Failed to find shader \"") + light + "\"";
        REG_CHECK_MSG(s.frames.count(expected.c_str()) == 1, "%s: expected exactly one \"%s\"; got:\n%s", light, expected.c_str(),
                      s.frames.text().c_str());
        bool noShaderCode = false;
        for (const SReport &r : s.frames.list)
            if (r.text.find(expected) != std::string::npos && r.code == CODE_NOSHADER)
                noShaderCode = true;
        REG_CHECK_MSG(noShaderCode, "%s: the error carries CODE_NOSHADER", light);
    }
    return finish("Hcshader_MissingShader");
}
