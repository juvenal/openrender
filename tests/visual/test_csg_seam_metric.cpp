/**
 * tests/visual/test_csg_seam_metric.cpp
 *
 * Seam-metric CLI tool for spec 021 (deferred, on-demand CSG evaluation).
 * Measures rendered solid (CSG) images against analytic expectations: clear
 * and solid regions, seam positions, projected intersection curves,
 * enclosed see-through holes, region diffs, chromaticity and luminance
 * ratios. Run with --help for the full usage.
 *
 * Every invocation runs one mode and prints one CSV summary line:
 *   mode=<mode>,<key>=<value>,...,result=PASS|FAIL
 * Exit codes: 0 = PASS, 1 = FAIL, 2 = usage or I/O error.
 *
 * Images are read as 8-bit RGBA TIFFs (same reader as test_visual_render.cpp);
 * colour values are premultiplied by alpha, as the renderer writes them.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include <png.h>
#include <tiffio.h>

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

struct Image {
        int width = 0;
        int height = 0;
        std::vector<float> rgba; // 0..1, premultiplied, top-to-bottom

        float at(int x, int y, int c) const { return rgba[4 * ((size_t)y * width + x) + c]; }
        float alpha(int x, int y) const { return at(x, y, 3); }
        float luma(int x, int y) const { return 0.2126f * at(x, y, 0) + 0.7152f * at(x, y, 1) + 0.0722f * at(x, y, 2); }
};

static bool readTiff(const char *path, Image &img) {
    TIFF *tif = TIFFOpen(path, "r");
    if (!tif) {
        fprintf(stderr, "Cannot open TIFF: %s\n", path);
        return false;
    }
    uint32_t w = 0, h = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    std::vector<uint32_t> raster((size_t)w * h);
    if (!TIFFReadRGBAImageOriented(tif, w, h, raster.data(), ORIENTATION_TOPLEFT, 0)) {
        fprintf(stderr, "Cannot decode TIFF: %s\n", path);
        TIFFClose(tif);
        return false;
    }
    TIFFClose(tif);
    img.width = (int)w;
    img.height = (int)h;
    img.rgba.resize(raster.size() * 4);
    for (size_t i = 0; i < raster.size(); ++i) {
        img.rgba[4 * i + 0] = TIFFGetR(raster[i]) / 255.0f;
        img.rgba[4 * i + 1] = TIFFGetG(raster[i]) / 255.0f;
        img.rgba[4 * i + 2] = TIFFGetB(raster[i]) / 255.0f;
        img.rgba[4 * i + 3] = TIFFGetA(raster[i]) / 255.0f;
    }
    return true;
}

// Grey-scale PNG mask: a pixel is inside where its value is >= 128.
struct Mask {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> value;
};

static bool readPngMask(const char *path, Mask &mask) {
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path)) {
        fprintf(stderr, "Cannot open PNG mask: %s\n", path);
        return false;
    }
    image.format = PNG_FORMAT_GRAY;
    mask.width = (int)image.width;
    mask.height = (int)image.height;
    mask.value.resize(PNG_IMAGE_SIZE(image));
    if (!png_image_finish_read(&image, nullptr, mask.value.data(), 0, nullptr)) {
        fprintf(stderr, "Cannot decode PNG mask: %s\n", path);
        png_image_free(&image);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Region shapes
// ---------------------------------------------------------------------------

enum class ShapeKind { Disc, Annulus, Rect, Frame, MaskFile };

struct Shape {
        ShapeKind kind = ShapeKind::Frame;
        double p[4] = {0, 0, 0, 0};
        Mask mask;
};

static bool parseNumbers(const std::string &text, std::vector<double> &out) {
    out.clear();
    size_t start = 0;
    while (start <= text.size()) {
        size_t comma = text.find(',', start);
        std::string token = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        char *end = nullptr;
        double v = strtod(token.c_str(), &end);
        if (token.empty() || end == token.c_str() || *end != '\0')
            return false;
        out.push_back(v);
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return true;
}

static bool parseShape(const std::string &text, Shape &shape) {
    std::vector<double> v;
    if (text == "frame") {
        shape.kind = ShapeKind::Frame;
        return true;
    }
    size_t colon = text.find(':');
    if (colon == std::string::npos)
        return false;
    std::string kind = text.substr(0, colon);
    std::string args = text.substr(colon + 1);
    if (kind == "mask") {
        shape.kind = ShapeKind::MaskFile;
        return readPngMask(args.c_str(), shape.mask);
    }
    if (!parseNumbers(args, v))
        return false;
    if (kind == "disc" && v.size() == 3) {
        shape.kind = ShapeKind::Disc;
    }
    else if (kind == "annulus" && v.size() == 4) {
        shape.kind = ShapeKind::Annulus;
    }
    else if (kind == "rect" && v.size() == 4) {
        shape.kind = ShapeKind::Rect;
    }
    else {
        return false;
    }
    for (size_t i = 0; i < v.size(); ++i)
        shape.p[i] = v[i];
    return true;
}

// Is the pixel centre (x+0.5, y+0.5) inside the shape, with its boundary
// moved inward by `inset` pixels (a negative inset grows the shape)? Masks
// ignore the inset.
static bool shapeContains(const Shape &s, const Image &img, int x, int y, double inset) {
    const double px = x + 0.5, py = y + 0.5;
    switch (s.kind) {
    case ShapeKind::Frame:
        return px >= inset && py >= inset && px <= img.width - inset && py <= img.height - inset;
    case ShapeKind::Disc: {
        const double d = std::hypot(px - s.p[0], py - s.p[1]);
        return d <= s.p[2] - inset;
    }
    case ShapeKind::Annulus: {
        const double d = std::hypot(px - s.p[0], py - s.p[1]);
        return d >= s.p[2] + inset && d <= s.p[3] - inset;
    }
    case ShapeKind::Rect:
        return px >= s.p[0] + inset && px <= s.p[2] - inset && py >= s.p[1] + inset && py <= s.p[3] - inset;
    case ShapeKind::MaskFile:
        if (x >= s.mask.width || y >= s.mask.height)
            return false;
        return s.mask.value[(size_t)y * s.mask.width + x] >= 128;
    }
    return false;
}

struct Region {
        std::vector<Shape> include;
        std::vector<Shape> exclude;
        double margin = 2.0;

        // Includes shrink by the margin, excludes grow by it.
        bool contains(const Image &img, int x, int y) const {
            bool in = include.empty() ? shapeContains(Shape{}, img, x, y, margin) : false;
            for (const Shape &s : include)
                if (shapeContains(s, img, x, y, margin)) {
                    in = true;
                    break;
                }
            if (!in)
                return false;
            for (const Shape &s : exclude)
                if (shapeContains(s, img, x, y, -margin))
                    return false;
            return true;
        }
};

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// What a seam or edge is a step in: alpha, Rec.709 luminance, or colour (the
// largest step in any of R, G, B, which sees a seam between two equally
// bright but differently coloured surfaces)
enum EStep { STEP_ALPHA = 0, STEP_LUMA, STEP_RGB };

static float channelValue(const Image &img, int x, int y, int channel) {
    return channel == 3 ? img.alpha(x, y) : channel == 4 ? img.luma(x, y) : img.at(x, y, channel);
}

// channel: 0-2 R, G, B; 3 alpha; 4 luminance
static float sampleBilinear(const Image &img, double x, double y, int channel) {
    x -= 0.5;
    y -= 0.5;
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    double fx = x - x0, fy = y - y0;
    auto value = [&](int xi, int yi) -> float {
        xi = std::clamp(xi, 0, img.width - 1);
        yi = std::clamp(yi, 0, img.height - 1);
        return channelValue(img, xi, yi, channel);
    };
    double top = value(x0, y0) * (1 - fx) + value(x0 + 1, y0) * fx;
    double bottom = value(x0, y0 + 1) * (1 - fx) + value(x0 + 1, y0 + 1) * fx;
    return (float)(top * (1 - fy) + bottom * fy);
}

// Edge pixels: an alpha crossing of 0.5, or a luminance (STEP_LUMA) or colour
// (STEP_RGB) step larger than `threshold`, between the pixel and one of its
// 4-neighbours.
static std::vector<uint8_t> detectEdges(const Image &img, float threshold, int step = STEP_LUMA) {
    std::vector<uint8_t> edge((size_t)img.width * img.height, 0);
    const int dx[2] = {1, 0}, dy[2] = {0, 1};
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            for (int k = 0; k < 2; ++k) {
                int nx = x + dx[k], ny = y + dy[k];
                if (nx >= img.width || ny >= img.height)
                    continue;
                bool alphaCross = (img.alpha(x, y) < 0.5f) != (img.alpha(nx, ny) < 0.5f);
                float diff = 0;
                if (step == STEP_RGB) {
                    for (int c = 0; c < 3; ++c)
                        diff = std::max(diff, std::fabs(img.at(x, y, c) - img.at(nx, ny, c)));
                }
                else
                    diff = std::fabs(img.luma(x, y) - img.luma(nx, ny));
                if (alphaCross || diff > threshold) {
                    edge[(size_t)y * img.width + x] = 1;
                    edge[(size_t)ny * img.width + nx] = 1;
                }
            }
    return edge;
}

// Distance from (cx, cy) to the nearest edge pixel centre within `radius`
// pixels; returns radius + 1 when none is found.
static double nearestEdge(const std::vector<uint8_t> &edge, int w, int h, double cx, double cy, int radius) {
    double best = radius + 1.0;
    int x0 = (int)std::floor(cx) - radius, x1 = (int)std::floor(cx) + radius;
    int y0 = (int)std::floor(cy) - radius, y1 = (int)std::floor(cy) + radius;
    for (int y = std::max(0, y0); y <= std::min(h - 1, y1); ++y)
        for (int x = std::max(0, x0); x <= std::min(w - 1, x1); ++x)
            if (edge[(size_t)y * w + x])
                best = std::min(best, std::hypot(x + 0.5 - cx, y + 0.5 - cy));
    return best;
}

static void printResult(const std::string &fields, bool pass) {
    printf("%s,result=%s\n", fields.c_str(), pass ? "PASS" : "FAIL");
}

static std::string fmt(const char *format, double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), format, v);
    return buf;
}

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------

struct Seam {
        double radius = 0;
        int step = STEP_ALPHA;
};

// (a) --annuli
static int runAnnuli(const Image &img, const Region &region, double cx, double cy, const std::vector<Shape> &clear,
                     const std::vector<Shape> &solid, const std::vector<Seam> &seams, double tolerance) {
    long stray = 0, gaps = 0;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            if (!region.contains(img, x, y))
                continue;
            for (const Shape &s : clear)
                if (shapeContains(s, img, x, y, region.margin) && img.alpha(x, y) >= 0.01f) {
                    ++stray;
                    break;
                }
            for (const Shape &s : solid)
                if (shapeContains(s, img, x, y, region.margin) && img.alpha(x, y) < 0.95f) {
                    ++gaps;
                    break;
                }
        }

    double maxDeviation = 0;
    std::string perSeam;
    const double window = std::max(3.0, 3.0 * tolerance);
    const int rays = 720;
    for (const Seam &seam : seams) {
        double seamMax = 0;
        int missing = 0;
        for (int i = 0; i < rays; ++i) {
            const double theta = 2.0 * M_PI * i / rays;
            const double ux = std::cos(theta), uy = std::sin(theta);
            double found = -1, bestGrad = 0;
            double prevR = seam.radius - window;
            // The channels the seam is measured on: alpha, luminance, or R, G and B
            const int first = seam.step == STEP_ALPHA ? 3 : seam.step == STEP_LUMA ? 4 : 0;
            const int last = seam.step == STEP_RGB ? 2 : first;
            float prevC[3];
            for (int c = first; c <= last; ++c)
                prevC[c - first] = sampleBilinear(img, cx + ux * prevR, cy + uy * prevR, c);
            float prev = prevC[0];
            for (double r = prevR + 0.25; r <= seam.radius + window; r += 0.25) {
                float curC[3];
                for (int c = first; c <= last; ++c)
                    curC[c - first] = sampleBilinear(img, cx + ux * r, cy + uy * r, c);
                float cur = curC[0];
                if (seam.step != STEP_ALPHA) {
                    double grad = 0;
                    for (int c = 0; c <= last - first; ++c)
                        grad = std::max(grad, std::fabs((double)curC[c] - prevC[c]) / 0.25);
                    if (grad > bestGrad) {
                        bestGrad = grad;
                        found = r - 0.125;
                    }
                    for (int c = 0; c <= last - first; ++c)
                        prevC[c] = curC[c];
                }
                else if ((prev < 0.5f) != (cur < 0.5f)) {
                    double crossing = r - 0.25 + 0.25 * (0.5 - prev) / (cur - prev);
                    if (found < 0 || std::fabs(crossing - seam.radius) < std::fabs(found - seam.radius))
                        found = crossing;
                }
                prev = cur;
            }
            if (seam.step != STEP_ALPHA && bestGrad < 0.02)
                found = -1; // no luminance or colour step at all along this ray
            if (found < 0)
                ++missing;
            const double deviation = found < 0 ? window : std::fabs(found - seam.radius);
            seamMax = std::max(seamMax, deviation);
        }
        maxDeviation = std::max(maxDeviation, seamMax);
        const char *suffix = seam.step == STEP_LUMA ? "_luma" : seam.step == STEP_RGB ? "_rgb" : "";
        perSeam += ",seam_" + fmt("%g", seam.radius) + suffix + "_max_px=" + fmt("%.3f", seamMax) + ",seam_" +
                   fmt("%g", seam.radius) + suffix + "_missing_rays=" +
                   std::to_string(missing);
    }

    const bool pass = stray == 0 && gaps == 0 && maxDeviation <= tolerance;
    printResult("mode=annuli,stray_pixels=" + std::to_string(stray) + ",gap_pixels=" + std::to_string(gaps) +
                    ",max_seam_deviation_px=" + fmt("%.3f", maxDeviation) + perSeam + ",tolerance_px=" +
                    fmt("%.3f", tolerance),
                pass);
    return pass ? 0 : 1;
}

// (b) --edge-compare
static int runEdgeCompare(const Image &cand, const Image &ref, const Region &region, double tolerance,
                          float lumaThreshold, int step) {
    if (cand.width != ref.width || cand.height != ref.height) {
        fprintf(stderr, "Image sizes differ\n");
        return 2;
    }
    // Hysteresis: a strong edge in one image (step > lumaThreshold) needs only
    // a weak edge (step > lumaThreshold / 3) nearby in the other, so an edge
    // whose step sits near the threshold cannot flicker in and out of one image.
    const std::vector<uint8_t> ec = detectEdges(cand, lumaThreshold, step);
    const std::vector<uint8_t> er = detectEdges(ref, lumaThreshold, step);
    const std::vector<uint8_t> ecWeak = detectEdges(cand, lumaThreshold / 3.0f, step);
    const std::vector<uint8_t> erWeak = detectEdges(ref, lumaThreshold / 3.0f, step);
    const int radius = (int)std::ceil(tolerance) + 3;
    double maxDistance = 0;
    long edgePixels = 0;
    for (int y = 0; y < cand.height; ++y)
        for (int x = 0; x < cand.width; ++x) {
            if (!region.contains(cand, x, y))
                continue;
            const size_t i = (size_t)y * cand.width + x;
            if (ec[i]) {
                ++edgePixels;
                maxDistance = std::max(maxDistance, nearestEdge(erWeak, ref.width, ref.height, x + 0.5, y + 0.5, radius));
            }
            if (er[i]) {
                ++edgePixels;
                maxDistance = std::max(maxDistance, nearestEdge(ecWeak, cand.width, cand.height, x + 0.5, y + 0.5, radius));
            }
        }
    const bool pass = maxDistance <= tolerance;
    printResult("mode=edge-compare,edge_pixels=" + std::to_string(edgePixels) + ",max_edge_distance_px=" +
                    fmt("%.3f", maxDistance) + ",tolerance_px=" + fmt("%.3f", tolerance),
                pass);
    return pass ? 0 : 1;
}

// (c) --region-compare: 8x8 block-average diff (0..255), as test_visual_render
static int runRegionCompare(const Image &a, const Image &b, const Region &region, double threshold) {
    if (a.width != b.width || a.height != b.height) {
        fprintf(stderr, "Image sizes differ\n");
        return 2;
    }
    const int block = 8;
    double maxDiff = 0;
    long blocks = 0, failBlocks = 0;
    for (int by = 0; by < a.height; by += block)
        for (int bx = 0; bx < a.width; bx += block) {
            double sa[4] = {0, 0, 0, 0}, sb[4] = {0, 0, 0, 0};
            int n = 0;
            for (int y = by; y < std::min(by + block, a.height); ++y)
                for (int x = bx; x < std::min(bx + block, a.width); ++x) {
                    if (!region.contains(a, x, y))
                        continue;
                    for (int c = 0; c < 4; ++c) {
                        sa[c] += a.at(x, y, c);
                        sb[c] += b.at(x, y, c);
                    }
                    ++n;
                }
            if (n == 0)
                continue;
            ++blocks;
            double d = 0;
            for (int c = 0; c < 4; ++c)
                d = std::max(d, 255.0 * std::fabs(sa[c] - sb[c]) / n);
            maxDiff = std::max(maxDiff, d);
            if (d > threshold)
                ++failBlocks;
        }
    const bool pass = failBlocks == 0;
    printResult("mode=region-compare,blocks=" + std::to_string(blocks) + ",fail_blocks=" + std::to_string(failBlocks) +
                    ",max_block_avg_diff=" + fmt("%.2f", maxDiff) + ",threshold=" + fmt("%.2f", threshold),
                pass);
    return pass ? 0 : 1;
}

// (d) --region-chroma
struct ShareBound {
        int channel = 0;
        double value = 0;
};

static int runRegionChroma(const Image &img, const Region &region, const double *expected, double tolerance,
                           const std::vector<ShareBound> &minShares, const std::vector<ShareBound> &maxShares) {
    double sum[3] = {0, 0, 0};
    long n = 0;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            if (!region.contains(img, x, y) || img.alpha(x, y) < 0.5f)
                continue;
            const double total = img.at(x, y, 0) + img.at(x, y, 1) + img.at(x, y, 2);
            if (total < 3.0 / 255.0)
                continue;
            for (int c = 0; c < 3; ++c)
                sum[c] += img.at(x, y, c) / total;
            ++n;
        }
    double share[3] = {0, 0, 0};
    bool pass = n > 0;
    for (int c = 0; c < 3; ++c)
        share[c] = n > 0 ? sum[c] / n : 0;
    if (expected)
        for (int c = 0; c < 3; ++c)
            if (std::fabs(share[c] - expected[c]) > tolerance)
                pass = false;
    for (const ShareBound &b : minShares)
        if (share[b.channel] < b.value)
            pass = false;
    for (const ShareBound &b : maxShares)
        if (share[b.channel] > b.value)
            pass = false;
    printResult("mode=region-chroma,pixels=" + std::to_string(n) + ",share_r=" + fmt("%.4f", share[0]) +
                    ",share_g=" + fmt("%.4f", share[1]) + ",share_b=" + fmt("%.4f", share[2]),
                pass);
    return pass ? 0 : 1;
}

// (e) --projected-curve
struct Circle {
        double c[3], n[3], r;
        bool hasClip = false;
        double clipMin[3], clipMax[3];
        std::vector<std::array<double, 3>> facing; // all must face the camera
};

static void transformPoint(const double m[16], const double p[3], double out[3]) {
    // RIB row-vector convention: p' = [x y z 1] M
    for (int j = 0; j < 3; ++j)
        out[j] = p[0] * m[0 * 4 + j] + p[1] * m[1 * 4 + j] + p[2] * m[2 * 4 + j] + m[3 * 4 + j];
}

static int runProjectedCurve(const Image &img, const Region &region, double fov, int w, int h, const double m[16],
                             const std::vector<Circle> &circles, double tolerance, float lumaThreshold, int step) {
    if (w != img.width || h != img.height) {
        fprintf(stderr, "--camera resolution %dx%d does not match the image %dx%d\n", w, h, img.width, img.height);
        return 2;
    }
    const std::vector<uint8_t> edge = detectEdges(img, lumaThreshold, step);
    const double scale = 0.5 * std::min(w, h) / std::tan(0.5 * fov * M_PI / 180.0);
    const int radius = (int)std::ceil(tolerance) + 3;
    const int samples = 720;
    double maxDeviation = 0;
    long used = 0;
    for (const Circle &ci : circles) {
        // Orthonormal basis (u, v) perpendicular to n
        double n[3] = {ci.n[0], ci.n[1], ci.n[2]};
        const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        for (double &k : n)
            k /= nl;
        double a[3] = {1, 0, 0};
        if (std::fabs(n[0]) > 0.9) {
            a[0] = 0;
            a[1] = 1;
        }
        double u[3] = {a[1] * n[2] - a[2] * n[1], a[2] * n[0] - a[0] * n[2], a[0] * n[1] - a[1] * n[0]};
        const double ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        for (double &k : u)
            k /= ul;
        double v[3] = {n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0]};
        std::vector<std::array<double, 3>> facingCam(ci.facing.size());
        for (size_t f = 0; f < ci.facing.size(); ++f)
            transformPoint(m, ci.facing[f].data(), facingCam[f].data());
        for (int i = 0; i < samples; ++i) {
            const double t = 2.0 * M_PI * i / samples;
            double p[3];
            for (int k = 0; k < 3; ++k)
                p[k] = ci.c[k] + ci.r * (std::cos(t) * u[k] + std::sin(t) * v[k]);
            if (ci.hasClip) {
                bool inside = true;
                for (int k = 0; k < 3; ++k)
                    if (p[k] < ci.clipMin[k] - 1e-9 || p[k] > ci.clipMax[k] + 1e-9)
                        inside = false;
                if (!inside)
                    continue;
            }
            double pc[3];
            transformPoint(m, p, pc);
            if (pc[2] <= 1e-9)
                continue;
            // (p - c) . (eye - p) > 0 for every --facing centre, eye at the
            // camera-space origin: the sample is on the camera side of each
            // convex operand, so none of them hides it
            bool visible = true;
            for (const auto &fc : facingCam) {
                double dot = 0;
                for (int k = 0; k < 3; ++k)
                    dot += (pc[k] - fc[k]) * (-pc[k]);
                if (dot <= 0)
                    visible = false;
            }
            if (!visible)
                continue;
            const double sx = 0.5 * w + scale * pc[0] / pc[2];
            const double sy = 0.5 * h - scale * pc[1] / pc[2];
            const int px = (int)std::floor(sx), py = (int)std::floor(sy);
            if (px < 0 || py < 0 || px >= w || py >= h || !region.contains(img, px, py))
                continue;
            ++used;
            maxDeviation = std::max(maxDeviation, nearestEdge(edge, w, h, sx, sy, radius));
        }
    }
    const bool pass = used > 0 && maxDeviation <= tolerance;
    printResult("mode=projected-curve,samples=" + std::to_string(used) + ",max_curve_deviation_px=" +
                    fmt("%.3f", maxDeviation) + ",tolerance_px=" + fmt("%.3f", tolerance),
                pass);
    return pass ? 0 : 1;
}

// (f) --enclosed-holes
static int runEnclosedHoles(const Image &img, const Region &region, double opaque) {
    const int w = img.width, h = img.height;
    std::vector<uint8_t> reached((size_t)w * h, 0);
    std::deque<std::pair<int, int>> queue;
    auto open = [&](int x, int y) { return img.alpha(x, y) < opaque; };
    for (int x = 0; x < w; ++x)
        for (int y : {0, h - 1})
            if (open(x, y) && !reached[(size_t)y * w + x]) {
                reached[(size_t)y * w + x] = 1;
                queue.emplace_back(x, y);
            }
    for (int y = 0; y < h; ++y)
        for (int x : {0, w - 1})
            if (open(x, y) && !reached[(size_t)y * w + x]) {
                reached[(size_t)y * w + x] = 1;
                queue.emplace_back(x, y);
            }
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!queue.empty()) {
        auto [x, y] = queue.front();
        queue.pop_front();
        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                continue;
            size_t i = (size_t)ny * w + nx;
            if (!reached[i] && open(nx, ny)) {
                reached[i] = 1;
                queue.emplace_back(nx, ny);
            }
        }
    }
    long holes = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (open(x, y) && !reached[(size_t)y * w + x] && region.contains(img, x, y))
                ++holes;
    const bool pass = holes == 0;
    printResult("mode=enclosed-holes,hole_pixels=" + std::to_string(holes) + ",opaque=" + fmt("%.3f", opaque), pass);
    return pass ? 0 : 1;
}

// (g) --luminance-ratio
static double meanLuminance(const Image &img, const Region &region, const Shape &shape, long &count) {
    double sum = 0;
    count = 0;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            if (!shapeContains(shape, img, x, y, region.margin))
                continue;
            bool excluded = false;
            for (const Shape &s : region.exclude)
                if (shapeContains(s, img, x, y, -region.margin))
                    excluded = true;
            if (excluded)
                continue;
            const float a = img.alpha(x, y);
            sum += a > 0.01f ? img.luma(x, y) / a : 0.0;
            ++count;
        }
    return count > 0 ? sum / count : 0.0;
}

static int runLuminanceRatio(const Image &img, const Region &region, const Shape &num, const Shape &den, double lo,
                             double hi) {
    long nn = 0, nd = 0;
    const double ln = meanLuminance(img, region, num, nn);
    const double ld = meanLuminance(img, region, den, nd);
    const double ratio = ld > 0 ? ln / ld : 0.0;
    const bool pass = nn > 0 && nd > 0 && ld > 0 && ratio >= lo && ratio <= hi;
    printResult("mode=luminance-ratio,numerator_pixels=" + std::to_string(nn) + ",denominator_pixels=" +
                    std::to_string(nd) + ",ratio=" + fmt("%.4f", ratio) + ",min=" + fmt("%.4f", lo) +
                    ",max=" + fmt("%.4f", hi),
                pass);
    return pass ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Command line
// ---------------------------------------------------------------------------

static void printHelp(const char *argv0) {
    printf(
        "Usage: %s <mode> <arguments> [region options]\n"
        "\n"
        "Measures a rendered solid (CSG) image. Prints one CSV summary line\n"
        "(mode=...,<metrics>,result=PASS|FAIL); exits 0 on PASS, 1 on FAIL,\n"
        "2 on a usage or I/O error. Images are 8-bit RGBA TIFFs.\n"
        "\n"
        "Region options (every mode):\n"
        "  --include <shape>   Restrict the check to this shape (repeatable;\n"
        "                      default: the whole frame).\n"
        "  --exclude <shape>   Remove this shape from the check (repeatable).\n"
        "  --margin <px>       Shrink includes and grow excludes by this many\n"
        "                      pixels to absorb pixel-filter falloff (default 2).\n"
        "  Shapes: disc:cx,cy,r  annulus:cx,cy,r0,r1  rect:x0,y0,x1,y1  frame\n"
        "          mask:<png>  (grey PNG; inside where the value is >= 128)\n"
        "  Coordinates are pixels, origin at the top-left corner.\n"
        "\n"
        "Modes:\n"
        "  --annuli <image> --center cx,cy [--clear <shape>]... [--solid <shape>]...\n"
        "          [--seam r | --seam r:luma | --seam r:rgb]... [--tolerance px]\n"
        "      Counts stray pixels (alpha >= 0.01) in --clear shapes and gap\n"
        "      pixels (alpha < 0.95) in --solid shapes; for each --seam radius,\n"
        "      finds the alpha 0.5 crossing (or, with :luma / :rgb, the strongest\n"
        "      luminance / colour step) along 720 rays and reports the largest radial\n"
        "      deviation. Passes when both counts are 0 and the deviation is\n"
        "      within --tolerance (default 1 px).\n"
        "  --edge-compare <candidate> <reference> [--tolerance px] [--edge-threshold l]\n"
        "          [--edge-mode luma|rgb]\n"
        "      Largest distance from each edge of either image (an alpha crossing\n"
        "      or a luminance step above --edge-threshold, default 0.05) to the\n"
        "      nearest weak edge (a third of the threshold) of the other image.\n"
        "      Passes within --tolerance (default 1 px).\n"
        "  --region-compare <a> <b> [--threshold t]\n"
        "      8x8 block-average RGBA difference (0..255) over the region, as the\n"
        "      visual regression suite measures it. Passes when no block exceeds\n"
        "      --threshold (default 20).\n"
        "  --region-chroma <image> [--rgb-share r g b --tolerance t]\n"
        "          [--min-share c=v]... [--max-share c=v]...\n"
        "      Mean chromaticity (each channel / (R+G+B)) of opaque pixels in the\n"
        "      region, checked against the expected shares (c is r, g or b).\n"
        "  --projected-curve <image> --camera <fov> <w> <h> <m00> ... <m33>\n"
        "          --circle cx,cy,cz,nx,ny,nz,r [--clip-box x0,y0,z0,x1,y1,z1]\n"
        "          [--facing cx,cy,cz] ... [--tolerance px] [--edge-threshold l]\n"        "          [--edge-mode luma|rgb]\n"
        "      Samples each exact 3D intersection circle (object space), keeps\n"
        "      the samples inside its --clip-box and, with --facing (repeatable),\n"
        "      on the camera-facing side of every convex operand centred there,\n"
        "      projects them through the perspective camera (<m..> is the\n"
        "      object-to-camera matrix in RIB row-vector order) and requires a\n"
        "      detected edge within --tolerance (default 1 px) of every one.\n"
        "      --clip-box and --facing apply to the preceding --circle.\n"
        "  --edge-mode luma|rgb  (edge-compare, projected-curve) what counts as an\n"
        "      edge besides an alpha crossing: a luminance step (default) or a\n"
        "      colour step (the largest change in R, G or B), which also sees a\n"
        "      seam between two equally bright, differently coloured surfaces.\n"
        "  --enclosed-holes <image> [--opaque a]\n"
        "      Counts pixels with alpha < --opaque (default 0.99) inside the\n"
        "      region that are not connected to the image border: see-through\n"
        "      gaps. Exclude legitimate openings with --exclude.\n"
        "  --luminance-ratio <image> --numerator <shape> --denominator <shape>\n"
        "          [--min r] [--max r]\n"
        "      Mean Rec.709 luminance of un-premultiplied RGB in the numerator\n"
        "      shape divided by that in the denominator shape; passes within\n"
        "      [--min, --max] (defaults 0 and infinity). For shadows and\n"
        "      reflections, which alpha cannot distinguish.\n",
        argv0);
}

static int usage(const char *message) {
    fprintf(stderr, "Error: %s (run with --help for usage)\n", message);
    return 2;
}

static int channelIndex(char c) {
    switch (c) {
    case 'r':
        return 0;
    case 'g':
        return 1;
    case 'b':
        return 2;
    default:
        return -1;
    }
}

int main(int argc, char **argv) {
    if (argc < 2)
        return usage("no mode given");
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp(argv[0]);
            return 0;
        }

    const std::string mode = argv[1];
    std::vector<std::string> images;
    Region region;
    double tolerance = 1.0, threshold = 20.0, opaque = 0.99, lo = 0.0, hi = HUGE_VAL;
    float edgeThreshold = 0.05f;
    int edgeStep = STEP_LUMA;
    double center[2] = {0, 0};
    bool hasCenter = false;
    std::vector<Shape> clear, solid;
    std::vector<Seam> seams;
    double expected[3];
    bool hasExpected = false;
    std::vector<ShareBound> minShares, maxShares;
    double fov = 0, matrix[16];
    int camW = 0, camH = 0;
    bool hasCamera = false;
    std::vector<Circle> circles;
    Shape numerator, denominator;
    bool hasNumerator = false, hasDenominator = false;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : nullptr; };
        if (arg == "--include" || arg == "--exclude" || arg == "--clear" || arg == "--solid" ||
            arg == "--numerator" || arg == "--denominator") {
            const char *v = next();
            Shape s;
            if (!v || !parseShape(v, s))
                return usage(("bad shape for " + arg).c_str());
            if (arg == "--include")
                region.include.push_back(s);
            else if (arg == "--exclude")
                region.exclude.push_back(s);
            else if (arg == "--clear")
                clear.push_back(s);
            else if (arg == "--solid")
                solid.push_back(s);
            else if (arg == "--numerator") {
                numerator = s;
                hasNumerator = true;
            }
            else {
                denominator = s;
                hasDenominator = true;
            }
        }
        else if (arg == "--margin" || arg == "--tolerance" || arg == "--threshold" || arg == "--opaque" ||
                 arg == "--min" || arg == "--max" || arg == "--edge-threshold") {
            const char *v = next();
            if (!v)
                return usage(("missing value for " + arg).c_str());
            const double d = atof(v);
            if (arg == "--margin")
                region.margin = d;
            else if (arg == "--tolerance")
                tolerance = d;
            else if (arg == "--threshold")
                threshold = d;
            else if (arg == "--opaque")
                opaque = d;
            else if (arg == "--min")
                lo = d;
            else if (arg == "--max")
                hi = d;
            else
                edgeThreshold = (float)d;
        }
        else if (arg == "--edge-mode") {
            const char *s = next();
            if (s && std::string(s) == "rgb")
                edgeStep = STEP_RGB;
            else if (s && std::string(s) == "luma")
                edgeStep = STEP_LUMA;
            else
                return usage("--edge-mode expects luma or rgb");
        }
        else if (arg == "--center") {
            std::vector<double> v;
            const char *s = next();
            if (!s || !parseNumbers(s, v) || v.size() != 2)
                return usage("--center expects cx,cy");
            center[0] = v[0];
            center[1] = v[1];
            hasCenter = true;
        }
        else if (arg == "--seam") {
            const char *s = next();
            if (!s)
                return usage("--seam expects a radius");
            Seam seam;
            std::string text = s;
            if (text.size() > 5 && text.substr(text.size() - 5) == ":luma") {
                seam.step = STEP_LUMA;
                text = text.substr(0, text.size() - 5);
            }
            else if (text.size() > 4 && text.substr(text.size() - 4) == ":rgb") {
                seam.step = STEP_RGB;
                text = text.substr(0, text.size() - 4);
            }
            seam.radius = atof(text.c_str());
            seams.push_back(seam);
        }
        else if (arg == "--rgb-share") {
            if (i + 3 >= argc)
                return usage("--rgb-share expects r g b");
            for (double &e : expected)
                e = atof(argv[++i]);
            hasExpected = true;
        }
        else if (arg == "--min-share" || arg == "--max-share") {
            const char *s = next();
            if (!s || strlen(s) < 3 || s[1] != '=' || channelIndex(s[0]) < 0)
                return usage((arg + " expects c=v with c in r, g, b").c_str());
            ShareBound b{channelIndex(s[0]), atof(s + 2)};
            (arg == "--min-share" ? minShares : maxShares).push_back(b);
        }
        else if (arg == "--camera") {
            if (i + 19 >= argc)
                return usage("--camera expects <fov> <w> <h> and 16 matrix values");
            fov = atof(argv[++i]);
            camW = atoi(argv[++i]);
            camH = atoi(argv[++i]);
            for (double &e : matrix)
                e = atof(argv[++i]);
            hasCamera = true;
        }
        else if (arg == "--circle") {
            std::vector<double> v;
            const char *s = next();
            if (!s || !parseNumbers(s, v) || v.size() != 7)
                return usage("--circle expects cx,cy,cz,nx,ny,nz,r");
            Circle c;
            for (int k = 0; k < 3; ++k) {
                c.c[k] = v[k];
                c.n[k] = v[3 + k];
            }
            c.r = v[6];
            circles.push_back(c);
        }
        else if (arg == "--clip-box" || arg == "--facing") {
            std::vector<double> v;
            const char *s = next();
            if (circles.empty())
                return usage((arg + " must follow a --circle").c_str());
            Circle &c = circles.back();
            if (arg == "--clip-box") {
                if (!s || !parseNumbers(s, v) || v.size() != 6)
                    return usage("--clip-box expects x0,y0,z0,x1,y1,z1");
                for (int k = 0; k < 3; ++k) {
                    c.clipMin[k] = v[k];
                    c.clipMax[k] = v[3 + k];
                }
                c.hasClip = true;
            }
            else {
                if (!s || !parseNumbers(s, v) || v.size() != 3)
                    return usage("--facing expects cx,cy,cz");
                c.facing.push_back({v[0], v[1], v[2]});
            }
        }
        else if (arg.rfind("--", 0) == 0) {
            return usage(("unknown option " + arg).c_str());
        }
        else {
            images.push_back(arg);
        }
    }

    const size_t needImages = (mode == "--edge-compare" || mode == "--region-compare") ? 2 : 1;
    if (images.size() != needImages)
        return usage("wrong number of images for this mode");
    std::vector<Image> loaded(images.size());
    for (size_t k = 0; k < images.size(); ++k)
        if (!readTiff(images[k].c_str(), loaded[k]))
            return 2;

    if (mode == "--annuli") {
        if (!hasCenter)
            return usage("--annuli needs --center");
        return runAnnuli(loaded[0], region, center[0], center[1], clear, solid, seams, tolerance);
    }
    if (mode == "--edge-compare")
        return runEdgeCompare(loaded[0], loaded[1], region, tolerance, edgeThreshold, edgeStep);
    if (mode == "--region-compare")
        return runRegionCompare(loaded[0], loaded[1], region, threshold);
    if (mode == "--region-chroma") {
        if (!hasExpected && minShares.empty() && maxShares.empty())
            return usage("--region-chroma needs --rgb-share or --min-share/--max-share");
        return runRegionChroma(loaded[0], region, hasExpected ? expected : nullptr, tolerance, minShares, maxShares);
    }
    if (mode == "--projected-curve") {
        if (!hasCamera || circles.empty())
            return usage("--projected-curve needs --camera and at least one --circle");
        return runProjectedCurve(loaded[0], region, fov, camW, camH, matrix, circles, tolerance, edgeThreshold, edgeStep);
    }
    if (mode == "--enclosed-holes")
        return runEnclosedHoles(loaded[0], region, opaque);
    if (mode == "--luminance-ratio") {
        if (!hasNumerator || !hasDenominator)
            return usage("--luminance-ratio needs --numerator and --denominator");
        return runLuminanceRatio(loaded[0], region, numerator, denominator, lo, hi);
    }
    return usage(("unknown mode " + mode).c_str());
}
