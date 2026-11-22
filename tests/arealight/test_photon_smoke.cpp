/**
 * Project: openRender
 *
 * File: tests/arealight/test_photon_smoke.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaLight_PhotonSmoke (contracts/tests.md; FR-013, SC-008; US8). tasks.md T116. A smoke check of the photon
 *   pass, never a parity subject: two renders of the scenes in tests/arealight/scenes/photon, each a separate orender process whose photon maps
 *   (.gpm) the test reads. A .gpm is the existing read-only observation of a photon pass (tests/gi_determinism/gi_detkit.h parses it): it holds
 *   where each photon was STORED (P) and the direction it arrived with (a byte pair), so the emission origin of a photon is recovered by
 *   tracing its stored direction back to the emitter's plane. The scenes stop a photon at its first hit (maxdiffusedepth 0), so a stored
 *   photon is a direct one.
 *
 *     arealight-photon-classic.rib  one spotlight at (5, 4, 5) aimed up at a patch at y = 8 (above.gpm); a floor at y = 0 (below.gpm)
 *     arealight-photon-bound.rib    the same plus a bound arealight on the quad x, z in [1, 3] at y = 3 facing down, as light 2; the light's own
 *                                   origin is (0, 3, 0), 1.41 away from the quad. Its photons land on the floor (below.gpm), the spotlight's on
 *                                   the patch above: the two never meet
 *
 *   PRE-DECLARED BOUNDS (fixed before any run, never adjusted):
 *     kDirError   0.03    the angle a stored direction can be off the emitted one: the byte pair truncates theta to pi/256 (0.0123 rad) and
 *                         phi to 2 pi/256 (0.0245 rad), so at most sqrt(0.0123^2 + 0.0245^2) = 0.0274; a recovered origin is off by at most
 *                         kDirError times the distance it was traced back (the photon-to-origin distance) divided by |dir.y| (the line meets
 *                         the emitter's plane at a slant: an error across the line moves the intersection by that error over the cosine of
 *                         the angle between the line and the plane's normal), plus 1e-3 for float arithmetic. (The first version of this bound
 *                         left the 1 / |dir.y| out: one of 1035 photons, a slanted one with |dir.y| = 0.36 traced back 8.35, was 1.35 times over it;
 *                         with the factor the worst photon is 0.60 of its bound, and the red state's origins, 1.41 from the quad, still fail)
 *     kMinBelow   100     the photons the bound light stores on the floor (2000 are emitted; about half reach the floor patch)
 *     kSpread     0.3     the standard deviation of the recovered origins along each in-plane axis (uniform over the 2 x 2 quad: 0.577);
 *                         a single origin point has 0
 *     kMeanTol    0.3     the mean of the recovered origins against the quad's centre (2, 3, 2)
 *
 *   Checks:
 *     1 both renders exit 0 and write above.gpm; the bound one also writes below.gpm
 *     2 calibration (the classic light, which this spec must not change): every photon of above.gpm (both scenes) traces back to the
 *       spotlight's position (5, 4, 5) to within the bound (point to line distance), which also validates the decode and the space of the file
 *     3 above.gpm of the two scenes is one and the same file, byte for byte (the classic emission is as before: the bound light adds no photon
 *       there and the spotlight keeps its index and keys)
 *     4 the bound light: below.gpm holds at least kMinBelow photons; every one arrived travelling down (dir.y < 0); every one traces back to
 *       a point on the quad (x and z in [1, 3] widened by the bound); the origins' spread and mean are those of a uniform quad
 *
 *   The .slo twin (AreaLight_PhotonSmoke-slo, tasks.md T159; FR-013): the same program with a fourth argument, the scene suffix "-slo", on
 *   arealight-photon-{classic,bound}-slo.rib, which are these scenes with Option "shaderformat" "default" ["slo"]; the bounds are the same.
 *
 *   Red on the Phase 7 tree: the pass emits from the light's origin (0, 3, 0) (or not at all): check 4 fails (origins not on the quad).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "gi_determinism/gi_detkit.h"

using namespace gidet;

namespace {

    const double kDirError = 0.03;
    const int kMinBelow = 100;
    const double kSpread = 0.3;
    const double kMeanTol = 0.3;
    const double kPi = 3.14159265358979323846;

    struct Photon {
            double P[3];
            double dir[3]; // the decoded travel direction (the quantized one the file holds)
    };

    // The photon item of a .gpm (gi_detkit.h kGpmItem = 44): P at 0, N at 12, flags at 24, C at 28, theta at 40, phi at 41
    bool readPhotons(const std::string &path, std::vector<Photon> &out) {
        out.clear();
        Bytes b;
        if (!gitest::readFile(path, b))
            return false;
        Gpm g;
        if (!parseGpm(b, g))
            return false;
        for (int i = 1; i <= g.numItems; i++) {
            const size_t base = g.itemsOff + kGpmItem * (size_t)i;
            float P[3];
            memcpy(P, &b[base], 12);
            const unsigned char t = b[base + 40], p = b[base + 41];
            // itemToDir (src/ri/core/map.h): the tables are sin/cos of t * pi / 256 and p * 2 pi / 256
            const double th = t * kPi / 256.0, ph = p * 2.0 * kPi / 256.0;
            Photon ph3;
            for (int k = 0; k < 3; k++)
                ph3.P[k] = P[k];
            ph3.dir[0] = std::sin(th) * std::cos(ph);
            ph3.dir[1] = std::sin(th) * std::sin(ph);
            ph3.dir[2] = std::cos(th);
            out.push_back(ph3);
        }
        return true;
    }

    // distance from `point` to the line through p along dir (unit), and the distance from p to the point
    double lineMiss(const Photon &p, const double point[3], double &dist) {
        double w[3] = {point[0] - p.P[0], point[1] - p.P[1], point[2] - p.P[2]};
        const double along = w[0] * p.dir[0] + w[1] * p.dir[1] + w[2] * p.dir[2];
        const double w2 = w[0] * w[0] + w[1] * w[1] + w[2] * w[2];
        dist = std::sqrt(w2);
        return std::sqrt(std::max(0.0, w2 - along * along));
    }

    // Traces the photon back along -dir to the plane y = planeY: the point and the distance travelled; false when the line never reaches it
    bool traceBackToPlane(const Photon &p, double planeY, double O[3], double &dist) {
        if (std::fabs(p.dir[1]) < 1e-9)
            return false;
        const double s = (planeY - p.P[1]) / (-p.dir[1]); // O = P - s dir
        if (s <= 0.0)
            return false;
        for (int k = 0; k < 3; k++)
            O[k] = p.P[k] - s * p.dir[k];
        dist = s;
        return true;
    }

    bool render(const std::string &orender, const std::string &scene, const std::string &dir, const char *label) {
        if (!makeDir(dir))
            return false;
        // a stale map of an earlier run must not be read: remove before rendering
        system(("rm -f \"" + dir + "/above.gpm\" \"" + dir + "/below.gpm\"").c_str());
        const RunResult r = runOrender(orender, dir, scene, "", dir + "/render.log");
        EXPECT(r.ok(), "%s: the render failed (%s)", label, describe(r).c_str());
        return r.ok();
    }

    void checkClassicCalibration(const std::vector<Photon> &above, const char *label) {
        const double spot[3] = {5.0, 4.0, 5.0};
        EXPECT(above.size() >= 100, "%s: above.gpm holds %zu photons, at least 100 expected from the spotlight", label, above.size());
        int bad = 0;
        double worst = 0.0;
        for (const Photon &p : above) {
            double d;
            const double miss = lineMiss(p, spot, d);
            const double tol = kDirError * d + 1e-3;
            worst = std::max(worst, miss / tol);
            if (!(miss <= tol))
                ++bad;
        }
        EXPECT(bad == 0, "%s: %d of %zu spotlight photons do not trace back to (5, 4, 5) (worst miss / bound = %.3f)", label, bad, above.size(), worst);
    }

    void checkBoundLight(const std::vector<Photon> &below) {
        EXPECT((int)below.size() >= kMinBelow, "the bound light stored %zu photons on the floor, at least %d expected", below.size(), kMinBelow);
        int notDown = 0, noPlane = 0, off = 0;
        double sx = 0, sz = 0, sxx = 0, szz = 0;
        int used = 0;
        double worstOut = 0.0;
        for (const Photon &p : below) {
            if (!(p.dir[1] < 0.0)) {
                ++notDown;
                continue;
            }
            double O[3], d;
            if (!traceBackToPlane(p, 3.0, O, d)) {
                ++noPlane;
                continue;
            }
            const double tol = kDirError * d / std::fabs(p.dir[1]) + 1e-3;
            // distance outside the quad [1, 3] x [1, 3] (0 inside)
            const double outX = std::max(std::max(1.0 - O[0], O[0] - 3.0), 0.0), outZ = std::max(std::max(1.0 - O[2], O[2] - 3.0), 0.0);
            const double out = std::sqrt(outX * outX + outZ * outZ);
            worstOut = std::max(worstOut, out / tol);
            if (out > tol)
                ++off;
            sx += O[0];
            sz += O[2];
            sxx += O[0] * O[0];
            szz += O[2] * O[2];
            ++used;
        }
        EXPECT(notDown == 0, "%d of %zu bound-light photons did not arrive travelling down (the quad faces -y)", notDown, below.size());
        EXPECT(noPlane == 0, "%d of %zu bound-light photons never trace back to the quad's plane", noPlane, below.size());
        EXPECT(off == 0, "%d of %zu bound-light photons trace back to a point off the quad (worst distance outside / bound = %.3f)", off, below.size(), worstOut);
        if (used > 0) {
            const double mx = sx / used, mz = sz / used;
            const double vx = sxx / used - mx * mx, vz = szz / used - mz * mz;
            const double dx = std::sqrt(std::max(0.0, vx)), dz = std::sqrt(std::max(0.0, vz));
            std::printf("  bound light: %zu photons, origin mean (%.3f, %.3f), spread (%.3f, %.3f)\n", below.size(), mx, mz, dx, dz);
            EXPECT(std::fabs(mx - 2.0) <= kMeanTol && std::fabs(mz - 2.0) <= kMeanTol, "the origins' mean (%.3f, %.3f) is not the quad's centre (2, 2)", mx, mz);
            EXPECT(dx >= kSpread && dz >= kSpread, "the origins' spread (%.3f, %.3f) is that of a point, not of a 2 x 2 quad (>= %.2f)", dx, dz, kSpread);
        }
    }

} // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Usage: %s <orender> <scenes dir> <work dir> [scene suffix, e.g. -slo]\n", argv[0]);
        return 2;
    }
    const std::string orender = argv[1], scenes = absolute(argv[2]), work = absolute(argv[3]);
    const std::string suffix = argc > 4 ? argv[4] : ""; // "-slo": the scenes of the .slo twin (the same scenes with the shaders loaded as .slo)
    const std::string dirC = work + "/classic", dirB = work + "/bound";
    const bool okC = render(orender, scenes + "/arealight-photon-classic" + suffix + ".rib", dirC, "classic scene");
    const bool okB = render(orender, scenes + "/arealight-photon-bound" + suffix + ".rib", dirB, "bound scene");
    if (okC && okB) {
        std::vector<Photon> aboveC, aboveB, below;
        EXPECT(readPhotons(dirC + "/above.gpm", aboveC), "classic scene: above.gpm is missing or does not parse");
        EXPECT(readPhotons(dirB + "/above.gpm", aboveB), "bound scene: above.gpm is missing or does not parse");
        EXPECT(readPhotons(dirB + "/below.gpm", below), "bound scene: below.gpm is missing or does not parse (the bound light stored no photon)");
        checkClassicCalibration(aboveC, "classic scene");
        checkClassicCalibration(aboveB, "bound scene");
        Bytes a, b;
        EXPECT(gitest::readFile(dirC + "/above.gpm", a) && gitest::readFile(dirB + "/above.gpm", b), "above.gpm of both scenes can be read");
        EXPECT(a == b, "above.gpm differs between the classic scene and the scene with a bound light: the classic emission changed (%zu and %zu bytes)", a.size(), b.size());
        checkBoundLight(below);
    }
    if (g_fail == 0)
        std::printf("AreaLight_PhotonSmoke%s: OK\n", suffix.c_str());
    else
        std::printf("AreaLight_PhotonSmoke%s: FAILED (%d)\n", suffix.c_str(), g_fail);
    return g_fail == 0 ? 0 : 1;
}
