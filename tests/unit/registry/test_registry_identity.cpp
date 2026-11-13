/**
 * Project: openRender
 *
 * File: test_registry_identity.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US3 AS1, FR-013, SC-005. Registry_IntegratorImageIdentity.
 *
 *   `Integrator "classic"` and no Integrator statement render byte-identical output with each camera hider
 *   (reyes, zbuffer, raytrace: the image file) and with the photon hider (the photon map file). One render thread
 *   keeps the photon pass deterministic. Each pair is guarded by a control: two renders with no statement are
 *   byte-identical, so a difference found is the statement's and not noise (the method of the project's visual-test
 *   noise note).
 *
 *   Green only once the Integrator statement parses (Phase 7): before, the statement is a syntax error of the RIB
 *   reader and the frame does not render.
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

namespace {

    std::string render(const std::string &hider, const std::string &integrator, const std::string &name) {
        const bool photon = hider == "photon";
        const std::string output = name + (photon ? ".gpm" : ".tif");
        removeFile(output);
        const std::string statement = integrator.empty() ? std::string() : "  Integrator \"" + integrator + "\"\n";
        std::string rib;
        if (photon) {
            rib = photonFrameText(1, statement, output);
        }
        else {
            SFrame f;
            f.statements = "  Hider \"" + hider + "\"\n" + statement;
            f.image = output;
            rib = frameText(f);
        }
        const SSession s = renderRib(rib, name + ".rib");
        REG_CHECK_MSG(s.frames.errors() == 0, "%s/%s: got:\n%s", hider.c_str(), integrator.c_str(), s.frames.text().c_str());
        REG_CHECK_MSG(fileExists(output), "%s/%s: wrote %s", hider.c_str(), integrator.c_str(), output.c_str());
        return fileExists(output) ? readFile(output) : std::string();
    }

} // namespace

int main() {
    for (const std::string hider : {"reyes", "zbuffer", "raytrace", "photon"}) {
        const std::string none1 = render(hider, "", "identity_" + hider + "_none1");
        const std::string none2 = render(hider, "", "identity_" + hider + "_none2");
        const std::string classic = render(hider, "classic", "identity_" + hider + "_classic");
        REG_CHECK_MSG(!none1.empty(), "%s: output not empty", hider.c_str());
        REG_CHECK_MSG(none1 == none2, "%s: control: two renders with no statement are identical", hider.c_str());
        REG_CHECK_MSG(classic == none1, "%s: Integrator \"classic\" equals no statement", hider.c_str());
    }
    return finish("Registry_IntegratorImageIdentity");
}
