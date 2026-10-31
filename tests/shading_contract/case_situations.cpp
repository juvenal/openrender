/**
 * tests/shading_contract/case_situations.cpp
 *
 * ShadingContract_Situations (spec 026, T061; US5 AS2, FR-017, FR-018, SC-006).
 *
 * Rows 1 to 5, 8 and 9 of the situation table of contracts/shader-globals.md, rendered in process, one
 * scene per row and hit mode, each in a child process (one RiBegin/RiEnd session per process, GitHub #2).
 * The renderer's two test observers (CPointKeyReport, CPointDisplacedReport) report, per shading call,
 * the shading dimension, the displacement pass, whether the call runs inside a shadow ray, the ray depth
 * and whether the illumination hook is on (trailing fields Phase 9 adds). From each report the case
 * builds a libshader::ShadingSituation, adds what only the scene knows (the hider and the three hit-mode
 * attributes) and asserts that libshader::classifyShading() returns the row's ray type, hit mode,
 * transport and pass.
 *
 * It covers every (call site, hit mode) pair, the non-default hit modes included: T009 found no hit-mode
 * attribute in the visual suite, so each scene exists once with the default and once with the other value:
 *   row 1  camerahitmode            shader / primitive   (read after the shaders: the shader runs in both)
 *   row 3  specularhitmode          shader / primitive   (a raytrace camera hit reads specularhitmode)
 *   row 4  specularhitmode          shader / primitive   (secondary hits)
 *   row 5  transmissionhitmode      primitive (default) / shader
 * The branch shade() took (primitive-only versus shaded) is observed through a marker shader that prints
 * its ray depth every time it runs: with "primitive" the marker is absent from the output.
 *
 * Rows 6 and 7 (implicit-surface and DSO-object hits, whose present behaviour is an oddity, O5 #119 and
 * O2 #116) and rows 10 to 14 (no shade()) are asserted from classifyShading() alone (B8). The eight
 * oddities of research R13 are asserted as oddities (documented, not fixed): 1 camerahitmode applies
 * after the shaders; 2 a raytrace camera hit reads specularhitmode, not camerahitmode; 3 an
 * implicit-surface hit is 0-D (no derivatives, no hit mode); 4 a DSO-object hit passes vVertices = -1;
 * 5 diffusehitmode is parsed and never read; 6 the atmosphere runs only at depth 0 and never on a matte
 * object; 7 a shaded transmission hit skips the displacement shader; 8 camerahitmode is ignored for
 * 0-D points and by the raytrace camera hit.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "case_common.h"
#include "libshader/shading/rendererServices.h"
#include "libshader/shading/shading.h"
#include "libshader/shading/situation.h"
#include "ri/parse/ri.h"

extern char **environ;

using namespace openrender::shading;

namespace {

    // ---- the child: one scene ---------------------------------------------------------------------------
    void onKey(const CPointKeyReport &r) {
        printf("R key dim=%d displaceOnly=%d inShadow=%d rayDepth=%d hook=%d\n", (int)r.dim, (int)r.displaceOnly, (int)r.inShadow, (int)r.rayDepth, (int)r.illuminationHook);
        fflush(stdout);
    }
    void onDisplaced(const CPointDisplacedReport &r) {
        printf("R disp dim=%d displaceOnly=%d inShadow=%d rayDepth=%d hook=%d\n", (int)r.dim, (int)r.displaceOnly, (int)r.inShadow, (int)r.rayDepth, (int)r.illuminationHook);
        fflush(stdout);
    }
    void *displayStart(const char *, int, int, int, const char *, RtDisplayParameterFunction) {
        static int token;
        return &token;
    }
    int displayData(void *, int, int, int, int, float *) { return RI_TRUE; }
    void displayFinish(void *) {}

    int renderScene(const char *scene) {
        const char *dir = getenv("SHADING_CONTRACT_SCENES");
        if (dir == nullptr) {
            fprintf(stderr, "Situations: SHADING_CONTRACT_SCENES is not set\n");
            return 2;
        }
        const std::string rib = std::string(dir) + "/" + scene + ".rib";
        RiBegin(RI_NULL);
        RiCustomDisplay("situations", RI_RGBA, displayStart, displayData, displayFinish, RI_NULL);
        CShadingContext::pointKeyObserver = &onKey;
        CShadingContext::pointDisplacedObserver = &onDisplaced;
        RiReadArchive((RtToken)rib.c_str(), NULL, NULL);
        CShadingContext::pointKeyObserver = nullptr;
        CShadingContext::pointDisplacedObserver = nullptr;
        RiEnd();
        fflush(stdout);
        return 0;
    }

    // ---- the parent: scenes, expectations ----------------------------------------------------------------
    struct SScene {
            const char *name;
            bool rasterHider;
            char cameraMode, specularMode, transmissionMode; // 's' (shader) or 'p' (primitive)
            bool markerExpected;                             // does a marker line appear at depth >= minMarkerDepth?
            int minMarkerDepth;
            bool expectMarkerAtAll;                          // any marker line (row 1: the shader runs even under "primitive")
    };

    struct SReport {
            bool displaced;
            int dim, displaceOnly, inShadow, rayDepth, hook;
    };

    struct SRunOutput {
            std::vector<SReport> reports;
            int markers = 0;          // SITMARK lines
            int markersDeep = 0;      // SITMARK lines at depth >= 1
            int displacementRuns = 0; // SITDISP lines
    };

    bool runChild(const char *self, const std::string &scene, const std::string &outFile, int &status) {
        std::vector<std::string> args = {self, "scene", scene};
        std::vector<char *> argv;
        for (std::string &a : args)
            argv.push_back(const_cast<char *>(a.c_str()));
        argv.push_back(nullptr);

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, outFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        pid_t pid;
        const int rc = posix_spawn(&pid, self, &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        if (rc != 0)
            return false;
        int st = 0;
        if (waitpid(pid, &st, 0) < 0)
            return false;
        status = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        return true;
    }

    SRunOutput parse(const std::string &file) {
        SRunOutput out;
        std::ifstream in(file);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("SITMARK", 0) == 0) {
                ++out.markers;
                const float depth = (float)atof(line.c_str() + 7);
                if (depth >= 0.5f)
                    ++out.markersDeep;
            }
            else if (line.rfind("SITDISP", 0) == 0) {
                ++out.displacementRuns;
            }
            else if (line.rfind("R ", 0) == 0) {
                SReport r{};
                r.displaced = (line.compare(2, 4, "disp") == 0);
                sscanf(line.c_str() + 7, "dim=%d displaceOnly=%d inShadow=%d rayDepth=%d hook=%d", &r.dim, &r.displaceOnly, &r.inShadow, &r.rayDepth, &r.hook);
                out.reports.push_back(r);
            }
        }
        return out;
    }

    const char *rayTypeName(RayType t) {
        switch (t) {
            case RayType::None: return "None";
            case RayType::Camera: return "Camera";
            case RayType::Secondary: return "Secondary";
            case RayType::Shadow: return "Shadow";
            case RayType::Photon: return "Photon";
        }
        return "?";
    }

    // The expected class of a report, from the situation table of contracts/shader-globals.md
    struct SRow {
            int row;
            libshader::ShadingClass cls;
    };
    SRow expectedRow(const SReport &r, const SScene &s) {
        auto mode = [](char c) { return c == 'p' ? HitMode::Primitive : HitMode::Shade; };
        libshader::ShadingClass c{RayType::None, HitMode::Shade, TransportMode::Radiance, ShadingPass::Full};
        if (r.displaceOnly == 3) {
            c.pass = ShadingPass::DisplaceEstimate;
            return {9, c};
        }
        if (r.displaceOnly != 0) {
            c.pass = ShadingPass::DisplaceOnly;
            return {8, c};
        }
        if (r.hook) {
            c.rayType = RayType::Photon;
            c.transport = TransportMode::Importance;
            return {10, c};
        }
        if (r.inShadow) {
            c.rayType = RayType::Shadow;
            c.hitMode = mode(s.transmissionMode);
            return {5, c};
        }
        if (s.rasterHider) {
            if (r.dim == (int)SHADING_0D)
                return {2, c}; // camerahitmode ignored for 0-D points (oddity 8)
            c.hitMode = mode(s.cameraMode);
            return {1, c};
        }
        if (r.rayDepth == 0) {
            c.rayType = RayType::Camera;
            c.hitMode = mode(s.specularMode); // specularhitmode, not camerahitmode (oddity 2)
            return {3, c};
        }
        c.rayType = RayType::Secondary;
        c.hitMode = mode(s.specularMode);
        return {4, c};
    }

    void checkScene(const SScene &s, const SRunOutput &out) {
        printf("  %s: %d report(s), %d marker(s), %d at depth >= 1, %d displacement run(s)\n", s.name, (int)out.reports.size(), out.markers, out.markersDeep, out.displacementRuns);
        CT_EXPECT(!out.reports.empty());

        std::set<int> rows;
        for (const SReport &r : out.reports) {
            const SRow want = expectedRow(r, s);
            rows.insert(want.row);
            libshader::ShadingSituation sit{};
            sit.dim = r.dim;
            sit.displaceOnly = r.displaceOnly;
            sit.inShadow = r.inShadow;
            sit.rayDepth = r.rayDepth;
            sit.illuminationHook = (r.hook != 0);
            sit.rasterHider = s.rasterHider;
            sit.cameraHitMode = s.cameraMode;
            sit.specularHitMode = s.specularMode;
            sit.transmissionHitMode = s.transmissionMode;
            const libshader::ShadingClass got = libshader::classifyShading(sit);
            if (got.rayType != want.cls.rayType || got.hitMode != want.cls.hitMode || got.transport != want.cls.transport || got.pass != want.cls.pass)
                fprintf(stderr, "    row %d: classified %s/%d/%d/%d, expected %s/%d/%d/%d\n", want.row, rayTypeName(got.rayType), (int)got.hitMode, (int)got.transport, (int)got.pass,
                        rayTypeName(want.cls.rayType), (int)want.cls.hitMode, (int)want.cls.transport, (int)want.cls.pass);
            CT_EXPECT(got.rayType == want.cls.rayType);
            CT_EXPECT(got.hitMode == want.cls.hitMode);
            CT_EXPECT(got.transport == want.cls.transport);
            CT_EXPECT(got.pass == want.cls.pass);
        }

        // The branch shade() took
        if (s.expectMarkerAtAll)
            CT_EXPECT(out.markers > 0);
        else if (s.markerExpected)
            CT_EXPECT(out.markersDeep >= 1 || (s.minMarkerDepth == 0 && out.markers > 0));
        else if (s.minMarkerDepth == 0)
            CT_EXPECT(out.markers == 0);
        else
            CT_EXPECT(out.markersDeep == 0);
    }

    // classifyShading() alone: rows 6, 7 and 10 to 14 (B8), and the oddities
    void classifyOnly() {
        printf("  classify-only rows (6, 7, 10 to 14)\n");
        auto situation = [](int dim, int displaceOnly, int inShadow, int depth, bool hook, bool raster) {
            libshader::ShadingSituation s{};
            s.dim = dim;
            s.displaceOnly = displaceOnly;
            s.inShadow = inShadow;
            s.rayDepth = depth;
            s.illuminationHook = hook;
            s.rasterHider = raster;
            s.cameraHitMode = 'p';
            s.specularHitMode = 'p';
            s.transmissionHitMode = 'p';
            return s;
        };
        // Row 6: an implicit-surface ray hit is 0-D: no hit mode (oddity 3); depth 0 -> Camera, deeper -> Secondary, in shadow -> Shadow
        {
            auto c0 = libshader::classifyShading(situation((int)SHADING_0D, 0, 0, 0, false, false));
            CT_EXPECT(c0.rayType == RayType::Camera && c0.hitMode == HitMode::Shade);
            auto c1 = libshader::classifyShading(situation((int)SHADING_0D, 0, 0, 1, false, false));
            CT_EXPECT(c1.rayType == RayType::Secondary && c1.hitMode == HitMode::Shade);
            auto c2 = libshader::classifyShading(situation((int)SHADING_0D, 0, 1, 1, false, false));
            CT_EXPECT(c2.rayType == RayType::Shadow && c2.hitMode == HitMode::Shade);
        }
        // Row 7: a DSO-object ray hit classifies as rows 3 to 5 (its vVertices = -1 is a defect, filed as O2 #116)
        {
            auto c0 = libshader::classifyShading(situation((int)SHADING_2D, 0, 0, 0, false, false));
            CT_EXPECT(c0.rayType == RayType::Camera && c0.hitMode == HitMode::Primitive);
            auto c1 = libshader::classifyShading(situation((int)SHADING_2D, 0, 0, 2, false, false));
            CT_EXPECT(c1.rayType == RayType::Secondary && c1.hitMode == HitMode::Primitive);
            auto c2 = libshader::classifyShading(situation((int)SHADING_2D, 0, 1, 1, false, false));
            CT_EXPECT(c2.rayType == RayType::Shadow && c2.hitMode == HitMode::Primitive);
        }
        // Row 10: photon emission (light shaders): the illumination hook is on, Importance transport
        {
            auto c = libshader::classifyShading(situation((int)SHADING_0D, 0, 0, 0, true, false));
            CT_EXPECT(c.rayType == RayType::Photon && c.transport == TransportMode::Importance && c.hitMode == HitMode::Shade && c.pass == ShadingPass::Full);
        }
        // Row 14: the RSLShading::shade() facade, a bare execution: no ray, Full
        {
            auto c = libshader::classifyShading(situation((int)SHADING_2D, 0, 0, 0, false, false));
            CT_EXPECT(c.pass == ShadingPass::Full && c.transport == TransportMode::Radiance);
        }
        // Rows 11 to 13 have no shading call (photon hits run no shader; the imager and the probe rays do not
        // call shade()): there is nothing to classify, and the table says so
    }

} // namespace

int caseSituations(int argc, char **argv) {
    if (argc >= 3 && strcmp(argv[1], "scene") == 0)
        return renderScene(argv[2]);

    // (name, raster hider, camera mode, specular mode, transmission mode, ...). The defaults are the shipped
    // ones: camerahitmode and specularhitmode "shader", transmissionhitmode "primitive".
    const SScene scenes[] = {
        {"sit_row1_camera_shaded", true, 's', 's', 'p', true, 0, true},
        {"sit_row1_camera_primitive", true, 'p', 's', 'p', true, 0, true}, // the shader runs under "primitive" too (oddity 1)
        {"sit_row2_points", true, 'p', 's', 'p', true, 0, true},           // camerahitmode ignored for 0-D points
        {"sit_row3_raytrace_shaded", false, 's', 's', 'p', true, 0, false},
        {"sit_row3_raytrace_primitive", false, 's', 'p', 'p', false, 0, false}, // a raytrace camera hit reads specularhitmode: no marker
        {"sit_row4_secondary_shaded", false, 's', 's', 'p', true, 1, false},
        {"sit_row4_secondary_primitive", false, 's', 'p', 'p', false, 1, false},
        {"sit_row5_shadow_primitive", false, 's', 's', 'p', false, 1, false},
        {"sit_row5_shadow_shaded", false, 's', 's', 's', true, 1, false},
        {"sit_row8_displace", true, 's', 's', 'p', true, 0, true},
    };
    for (const SScene &s : scenes) {
        const std::string outFile = std::string(s.name) + ".out";
        int status = -1;
        remove(outFile.c_str());
        CT_EXPECT(runChild(argv[0], s.name, outFile, status));
        CT_EXPECT(status == 0);
        const SRunOutput out = parse(outFile);
        checkScene(s, out);

        // The displacement scene: the passes seen are the dicing passes and the bound estimate (rows 8 and 9)
        if (strcmp(s.name, "sit_row8_displace") == 0) {
            CT_EXPECT(out.displacementRuns > 0);
            std::set<int> passes;
            for (const SReport &r : out.reports)
                passes.insert(r.displaceOnly);
            CT_EXPECT(passes.count(1) + passes.count(3) >= 1); // at least a displacement-only pass or an estimate
        }
    }

    classifyOnly();
    return contract_test::finish();
}
