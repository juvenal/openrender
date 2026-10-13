/**
 * tests/visual/test_depth_analytic.cpp
 *
 * Depth-output accuracy driver (fix/raytrace-midpoint-depth, GitHub #47, #49).
 *
 * Renders a scene of spheres once and checks every pixel's "z" against the
 * exact ray-sphere depths, so neither hider serves as the other's reference.
 * The scene must sample each pixel once at its centre (PixelSamples 1 1,
 * PixelFilter "box" 1 1, jitter off) through a perspective camera at the
 * camera-space origin looking down +z.
 *
 *   min: z is the camera-space depth of the nearest surface. A hider that
 *        writes the distance along the ray instead is off by 1/cos of the
 *        ray's angle to the view axis (#49).
 *   mid: z is the midpoint of the nearest and second-nearest surfaces. With
 *        no second surface it is 0.5 * (nearest + clipMax), Pixie's
 *        convention: nothing behind the only surface can receive a shadow.
 *        A hider that stops at the first opaque hit never sees the second
 *        surface (#47).
 *
 * A pixel with no surface must read exactly C_INFINITY. Pixels whose ray
 * grazes a sphere's silhouette are skipped: tessellation moves the reyes
 * silhouette by a fraction of a pixel.
 *
 * Usage:
 *   test_depth_analytic <orender> <rib> <output_tif> <min|mid> <fov> <sides>
 *                       <cx,cy,cz,r>...
 *
 * Sphere centres are in camera space. With <sides> 1 only a sphere's front
 * (entry) surface is visible; with 2 its back (exit) surface is as well.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "depth_tiff.h"

// Default far clip (CRenderer::clipMax): the midpoint floor with no second
// surface.
static constexpr float RENDERER_CLIPMAX = 1e30f;

// Relative tolerance on a covered pixel's depth: reyes's tessellation error on
// these scenes is under 0.06% away from silhouettes, and #49's error is 0.9%
// eight pixels off-axis.
static constexpr double DEPTH_TOLERANCE = 1e-3;

// Skip pixels whose ray passes within this fraction of a radius of a sphere's
// silhouette, where the surface turns away from the camera and reyes's
// micropolygon chords move z the most: measured up to 2.4e-3 within 5% of a
// radius, under 5.4e-4 beyond 10%.
static constexpr double GRAZE_FRACTION = 0.1;

// A test that checks too few covered pixels proves nothing.
static constexpr int MIN_COVERED_PIXELS = 100;

struct Sphere {
    double c[3];
    double r;
};

static bool parseSphere(const char *arg, Sphere &s) {
    return sscanf(arg, "%lf,%lf,%lf,%lf", &s.c[0], &s.c[1], &s.c[2], &s.r) == 4 && s.r > 0;
}

int main(int argc, char *argv[]) {
    if (argc < 8) {
        fprintf(stderr, "Usage: %s <orender> <rib> <output_tif> <min|mid> <fov> <sides> <cx,cy,cz,r>...\n",
                argv[0]);
        return 1;
    }
    const char *orender = argv[1];
    const char *rib = argv[2];
    const char *outputTif = argv[3];
    const std::string mode = argv[4];
    const double fov = atof(argv[5]);
    const int sides = atoi(argv[6]);
    if ((mode != "min" && mode != "mid") || fov <= 0 || (sides != 1 && sides != 2)) {
        fprintf(stderr, "  bad mode, fov or sides\n");
        return 1;
    }
    std::vector<Sphere> spheres;
    for (int i = 7; i < argc; i++) {
        Sphere s;
        if (!parseSphere(argv[i], s)) {
            fprintf(stderr, "  bad sphere \"%s\"\n", argv[i]);
            return 1;
        }
        spheres.push_back(s);
    }

    const std::string command = std::string("\"") + orender + "\" \"" + rib + "\"";
    printf("Running: %s\n", command.c_str());
    DepthImage img;
    if (!renderScene(command, outputTif) || !readDepthTiff(outputTif, img))
        return 1;

    // Camera-space direction through a pixel centre, scaled so its z is 1:
    // the ray parameter at a hit is then the hit's camera-space depth. The
    // field of view spans the image's shorter side.
    const double slope = tan(fov * M_PI / 360.0) / (0.5 * std::min(img.width, img.height));

    int covered = 0, background = 0, skipped = 0, failures = 0;
    for (uint32_t py = 0; py < img.height; py++) {
        for (uint32_t px = 0; px < img.width; px++) {
            const double d[3] = {(px + 0.5 - 0.5 * img.width) * slope, (0.5 * img.height - py - 0.5) * slope, 1.0};
            const double dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];

            std::vector<double> surfaces;
            bool grazing = false;
            for (const Sphere &s : spheres) {
                // |t d - c|^2 = r^2  ->  dd t^2 - 2 (d.c) t + (c.c - r^2) = 0
                const double dc = d[0] * s.c[0] + d[1] * s.c[1] + d[2] * s.c[2];
                const double cc = s.c[0] * s.c[0] + s.c[1] * s.c[1] + s.c[2] * s.c[2];
                const double closest = sqrt(std::max(0.0, cc - dc * dc / dd));
                if (fabs(closest - s.r) < GRAZE_FRACTION * s.r)
                    grazing = true;
                const double disc = dc * dc - dd * (cc - s.r * s.r);
                if (disc <= 0)
                    continue;
                const double root = sqrt(disc);
                const double entry = (dc - root) / dd;
                const double exit = (dc + root) / dd;
                if (entry > 0)
                    surfaces.push_back(entry);
                if (sides == 2 && exit > 0)
                    surfaces.push_back(exit);
            }
            if (grazing) {
                skipped++;
                continue;
            }
            std::sort(surfaces.begin(), surfaces.end());

            const float z = img.z[(size_t)py * img.width + px];
            if (surfaces.empty()) {
                background++;
                if (z != RENDERER_INFINITY) {
                    if (failures < 8)
                        fprintf(stderr, "  pixel (%u, %u): z = %.9g, expected background C_INFINITY\n", px, py, z);
                    failures++;
                }
                continue;
            }

            covered++;
            double expected = surfaces[0];
            if (mode == "mid")
                expected = 0.5 * (surfaces[0] + (surfaces.size() > 1 ? surfaces[1] : (double)RENDERER_CLIPMAX));
            if (!(fabs(z - expected) <= DEPTH_TOLERANCE * expected)) {
                if (failures < 8)
                    fprintf(stderr, "  pixel (%u, %u): z = %.9g, expected %.9g (%zu surface(s))\n", px, py, z,
                            expected, surfaces.size());
                failures++;
            }
        }
    }

    printf("  %d covered, %d background, %d grazing (skipped)\n", covered, background, skipped);
    if (covered < MIN_COVERED_PIXELS) {
        fprintf(stderr, "  FAIL: only %d covered pixels checked (need %d)\n", covered, MIN_COVERED_PIXELS);
        return 1;
    }
    if (failures > 0) {
        fprintf(stderr, "  FAIL: %d pixel(s) off the analytic depth\n", failures);
        return 1;
    }
    printf("  PASS: every pixel within %g of the analytic %s depth\n", DEPTH_TOLERANCE, mode.c_str());
    return 0;
}
