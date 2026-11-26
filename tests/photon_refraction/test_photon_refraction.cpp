/**
 * tests/photon_refraction/test_photon_refraction.cpp
 *
 * GitHub #211: the photon hider's "glass", "water" and "dielectric" shading models refract with the index ratio inverted. RISpec 3.2 (and
 * mathSpec.h's refract() and fresnel(), which follow it) take eta as the index of the medium the ray is in over the index of the medium it
 * enters, so a photon entering glass from air needs 1/ior and a photon leaving it needs ior. Each test below renders a photon pass with
 * orender, reads the caustic map the pass wrote and checks a physical property of it, never a recorded image:
 *
 *   entry   <model>   one sheet whose normal faces the light, photons arriving at 45 degrees: the stored direction is Snell's law,
 *                     asin(sin(45) / ior), not the incoming direction
 *   exit    <model>   one sheet tilted 20 degrees about y whose normal points along the photons' travel, photons arriving along +z: the stored
 *                     direction is Snell's law for a ray leaving a medium of index ior into air
 *   focus   <model>   a ball lens lit by a point light, and a receiver at the paraxial image plane: the radial distribution of the caustic map
 *                     is the one an exact ray trace through the two surfaces gives
 *   dispersion        the exit sheet with `iorrange [1.4 1.8]` and a sampled spectrum: the red end of the spectrum must see the first index
 *                     and the blue end the second (the index each photon saw is recovered from its exit angle by Snell's law)
 *
 * Usage: test_photon_refraction <mode> <model> <orender> <scenes dir> <work dir>
 *   model: glass | water | dielectric  (dielectric uses Attribute "photon" "float ior" [1.4])
 * The scenes are the .rib.in templates in tests/photon_refraction/scenes, with their @NAME@ fields filled in here. Standard library only.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static int g_fail = 0;
#define EXPECT(cond, ...)                               \
    do {                                                \
        if (!(cond)) {                                  \
            g_fail++;                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static const double kPi = 3.14159265358979323846;

struct Vec {
        double x, y, z;
};
static double dot(const Vec &a, const Vec &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec norm(const Vec &a) {
    const double l = std::sqrt(dot(a, a));
    return {a.x / l, a.y / l, a.z / l};
}

// Snell's law for a unit I, a unit N (either side) and the indices n1 (where the ray is) and n2 (what it enters). Written from the physics, not from
// refract(): T = n I + (n c - sqrt(1 - n^2 (1 - c^2))) N' with N' the normal facing the ray, c = -I.N' and n = n1 / n2.
static bool snell(const Vec &I, Vec N, double n1, double n2, Vec &T) {
    if (dot(I, N) > 0) {
        N = {-N.x, -N.y, -N.z};
    }
    const double n = n1 / n2, c = -dot(I, N);
    const double k = 1 - n * n * (1 - c * c);
    if (k <= 0) {
        return false;
    }
    const double a = n * c - std::sqrt(k);
    T = norm({n * I.x + a * N.x, n * I.y + a * N.y, n * I.z + a * N.z});
    return true;
}

// Unpolarized Fresnel reflectance for a ray with cosine ci to the normal going from index n1 into n2 (the hider chooses reflection with this probability)
static double fresnelReflectance(double ci, double n1, double n2) {
    const double st2 = (n1 / n2) * (n1 / n2) * (1 - ci * ci);
    if (st2 >= 1) {
        return 1;
    }
    const double ct = std::sqrt(1 - st2);
    const double rs = (n1 * ci - n2 * ct) / (n1 * ci + n2 * ct), rp = (n1 * ct - n2 * ci) / (n1 * ct + n2 * ci);
    return 0.5 * (rs * rs + rp * rp);
}

struct Photon {
        Vec P, dir, C;
};

// A photon map file (.gpm/.cpm layout, tests/gi_determinism/gi_detkit.h): header, numItems, maxItems, then numItems + 1 items of 44 bytes (item 0
// first); an item is P (3 floats), N (3), flags (2 bytes), 2 pad, C (3 floats), theta, phi (one byte each), 2 pad. The direction is quantized to 256
// steps of theta (0 to pi) and of phi (0 to 2 pi), decoded here at the middle of its step.
static bool readMap(const std::string &path, std::vector<Photon> &out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 40) {
        return false;
    }
    int32_t n = 0;
    memcpy(&n, &d[20], 4);
    if (n < 0 || n > 64) {
        return false;
    }
    const size_t header = 24 + (size_t)n + 1;
    int32_t num = 0;
    memcpy(&num, &d[header], 4);
    const size_t items = header + 8;
    if (num < 0 || d.size() < items + 44 * (size_t)(num + 1)) {
        return false;
    }
    for (int i = 1; i <= num; i++) {
        const unsigned char *p = &d[items + 44 * (size_t)i];
        float P[3], C[3];
        memcpy(P, p, 12);
        memcpy(C, p + 28, 12);
        const double th = (p[40] + 0.5) * kPi / 256.0, ph = (p[41] + 0.5) * 2 * kPi / 256.0;
        out.push_back({{P[0], P[1], P[2]}, {std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th)},
                       {C[0], C[1], C[2]}});
    }
    return true;
}

static std::string readText(const std::string &path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

static std::string replaceAll(std::string s, const std::string &from, const std::string &to) {
    for (size_t at = 0; (at = s.find(from, at)) != std::string::npos; at += to.size()) {
        s.replace(at, from.size(), to);
    }
    return s;
}

struct Model {
        std::string lines; // the RIB attribute lines
        double ior;
};

static bool modelFor(const std::string &name, Model &m) {
    if (name == "glass") {
        m = {"Attribute \"photon\" \"string shadingmodel\" [\"glass\"]", 1.5};
    }
    else if (name == "water") {
        m = {"Attribute \"photon\" \"string shadingmodel\" [\"water\"]", 1.3333333};
    }
    else if (name == "dielectric") {
        m = {"Attribute \"photon\" \"string shadingmodel\" [\"dielectric\"] \"float ior\" [1.4]", 1.4};
    }
    else {
        return false;
    }
    return true;
}

static std::string g_orender, g_scenes, g_work;

static std::string num(double v) {
    char b[64];
    snprintf(b, sizeof(b), "%.9g", v);
    return b;
}

// Fill in the template, render it (a photon pass only) and read the caustic map
static bool render(const char *tmpl, const std::string &tag, const std::vector<std::pair<std::string, std::string>> &fields, std::vector<Photon> &photons) {
    std::string rib = readText(g_scenes + "/" + tmpl);
    if (rib.empty()) {
        printf("cannot read %s/%s\n", g_scenes.c_str(), tmpl);
        return false;
    }
    const std::string map = tag + ".cpm";
    for (const auto &f : fields) {
        rib = replaceAll(rib, "@" + f.first + "@", f.second);
    }
    rib = replaceAll(rib, "@MAP@", map);
    rib = replaceAll(rib, "@HIDEREXTRA@", "");
    const std::string path = g_work + "/" + tag + ".rib";
    std::ofstream(path) << rib;
    remove((g_work + "/" + map).c_str());
    const std::string cmd = "cd \"" + g_work + "\" && \"" + g_orender + "\" \"" + path + "\" > \"" + g_work + "/" + tag + ".log\" 2>&1";
    const int rc = system(cmd.c_str());
    if (rc != 0) {
        printf("orender failed (%d), see %s/%s.log\n", rc, g_work.c_str(), tag.c_str());
        return false;
    }
    if (!readMap(g_work + "/" + map, photons)) {
        printf("no readable %s\n", map.c_str());
        return false;
    }
    return true;
}

static double angleDeg(const Vec &a, const Vec &b) { return std::acos(std::max(-1.0, std::min(1.0, dot(a, b)))) * 180 / kPi; }

// Median angle between the stored directions and the expected one, and how many photons were stored
static void checkDirection(const std::vector<Photon> &ph, const Vec &expect, double tolDeg, size_t minPhotons) {
    std::vector<double> err;
    for (const auto &p : ph) {
        err.push_back(angleDeg(p.dir, expect));
    }
    std::sort(err.begin(), err.end());
    const double med = err.empty() ? 180 : err[err.size() / 2];
    printf("%zu photons stored; expected direction (%.4f %.4f %.4f); median error %.2f degrees (tolerance %.2f)\n", ph.size(), expect.x, expect.y, expect.z, med,
           tolDeg);
    EXPECT(ph.size() >= minPhotons, "only %zu photons reached the receiver after the refraction (at least %zu expected)", ph.size(), minPhotons);
    EXPECT(med <= tolDeg, "the stored photon directions are %.2f degrees from Snell's law (at most %.2f allowed)", med, tolDeg);
}

static int modeEntry(const Model &m) {
    std::vector<Photon> ph;
    // light travels along (1 0 1); ReverseOrientation puts the sheet's normal on the side the light comes from: the photons enter the object
    if (!render("refract-plane.rib.in", "entry",
                {{"EMIT", "40000"}, {"LIGHT_TO", "1 0 1"}, {"ORIENT", "ReverseOrientation"}, {"MODEL", m.lines}, {"FACE", "-12 -12 2  12 -12 2  12 12 2  -12 12 2"}},
                ph)) {
        return 2;
    }
    Vec T{};
    snell(norm({1, 0, 1}), {0, 0, -1}, 1.0, m.ior, T);
    checkDirection(ph, T, 1.5, 200);
    return g_fail ? 1 : 0;
}

static int modeExit(const Model &m) {
    std::vector<Photon> ph;
    const double a = 20 * kPi / 180, t = std::tan(a);
    // the sheet is tilted about y; its normal (sin a, 0, cos a) points along the photons' travel: the photons leave the object
    const std::string face = "-3 -3 " + num(4 + 3 * t) + "  3 -3 " + num(4 - 3 * t) + "  3 3 " + num(4 - 3 * t) + "  -3 3 " + num(4 + 3 * t);
    if (!render("refract-plane.rib.in", "exit", {{"EMIT", "40000"}, {"LIGHT_TO", "0 0 1"}, {"ORIENT", ""}, {"MODEL", m.lines}, {"FACE", face}}, ph)) {
        return 2;
    }
    Vec T{};
    if (!snell({0, 0, 1}, {std::sin(a), 0, std::cos(a)}, m.ior, 1.0, T)) {
        return 2;
    }
    checkDirection(ph, T, 1.5, 200);
    return g_fail ? 1 : 0;
}

// Image of a point source on the axis, distance d from the centre of a ball of radius R and index n, by the paraxial formula for each surface
static double imagePlane(double d, double R, double n) {
    const double s = d - R;
    const double s1 = n / ((n - 1) / R - 1 / s);   // the image of the front surface, measured from the front vertex
    const double s2 = -(s1 - 2 * R);               // the object of the back surface (virtual: negative), from the back vertex
    const double s2i = 1 / ((n - 1) / R - n / s2); // the image of the back surface, from the back vertex
    return d + R + s2i;
}

// The same experiment by exact ray tracing. Rays leave the point light uniformly in solid angle over the cone that hits the ball; at each
// surface the path splits into a transmitted and a reflected branch weighted by the Fresnel reflectance (the chance the hider draws for each),
// up to the hider's four specular events; a branch that leaves the ball toward the receiver lands on its plane. Returns the fraction of the
// landed weight within each radius of the axis.
struct Reference {
        double d, R, n, zImage;
        const std::vector<double> *radii;
        std::vector<double> within;
        double landed = 0;
        static const int kMaxEvents = 4;

        void follow(Vec P, Vec D, bool inside, int events, double w) {
            // distance to the surface along the ray (the far side from inside, the near side from outside), or none
            const Vec Q{P.x, P.y, P.z - d};
            const double b = -dot(Q, D), c = dot(Q, Q) - R * R, disc = b * b - c;
            double t = -1;
            if (inside) {
                t = b + std::sqrt(std::max(disc, 0.0));
            }
            else if (disc > 0 && b > 0) {
                t = b - std::sqrt(disc);
            }
            if (t <= 0) {
                if (events > 0 && D.z > 0) { // out of the ball, on to the receiver
                    const double t3 = (zImage - P.z) / D.z;
                    const double r = std::hypot(P.x + t3 * D.x, P.y + t3 * D.y);
                    landed += w;
                    for (size_t k = 0; k < radii->size(); k++) {
                        if (r < (*radii)[k]) {
                            within[k] += w;
                        }
                    }
                }
                return;
            }
            if (events >= kMaxEvents) {
                return;
            }
            const Vec H{P.x + t * D.x, P.y + t * D.y, P.z + t * D.z};
            const Vec N = norm({H.x, H.y, H.z - d}); // outward
            const double n1 = inside ? n : 1.0, n2 = inside ? 1.0 : n;
            const double kr = fresnelReflectance(std::fabs(dot(D, N)), n1, n2);
            Vec T{};
            if (kr < 1 && snell(D, N, n1, n2, T)) {
                follow(H, T, !inside, events + 1, w * (1 - kr));
            }
            const double dn = dot(D, N);
            follow(H, {D.x - 2 * dn * N.x, D.y - 2 * dn * N.y, D.z - 2 * dn * N.z}, inside, events + 1, w * kr);
        }
};

static std::vector<double> referenceFractions(double d, double R, double n, double zImage, const std::vector<double> &radii) {
    const int kRays = 200000;
    const double cosMax = std::sqrt(1 - (R / d) * (R / d));
    Reference ref{d, R, n, zImage, &radii, std::vector<double>(radii.size(), 0.0)};
    for (int i = 0; i < kRays; i++) {
        const double cs = 1 - (1 - cosMax) * (i + 0.5) / kRays; // cos(theta), uniform in solid angle
        const double ph = kPi * (3 - std::sqrt(5.0)) * i;       // golden-angle spiral
        const double sn = std::sqrt(1 - cs * cs);
        ref.follow({0, 0, 0}, {sn * std::cos(ph), sn * std::sin(ph), cs}, false, 0, 1.0);
    }
    std::vector<double> f;
    for (size_t k = 0; k < radii.size(); k++) {
        f.push_back(ref.within[k] / ref.landed);
    }
    return f;
}

static int modeFocus(const Model &m) {
    std::vector<Photon> ph;
    const double R = 0.8, d = 9.0, z = imagePlane(d, R, m.ior);
    if (!render("ball-lens.rib.in", "focus", {{"EMIT", "1000000"}, {"ORIENT", ""}, {"MODEL", m.lines}, {"R", num(R)}, {"D", num(d)}, {"ZIMG", num(z)}}, ph)) {
        return 2;
    }
    // The whole front of the ball is lit, so the spot has the ball's spherical aberration: compare the radial distribution of the caustic map with
    // that of the exact ray trace, at three radii, rather than asking for a point.
    const std::vector<double> radii = {0.1, 0.25, 0.5};
    const std::vector<double> expect = referenceFractions(d, R, m.ior, z, radii);
    printf("ball lens, ior %.4f, paraxial image plane z = %.4f: %zu caustic photons\n", m.ior, z, ph.size());
    EXPECT(ph.size() >= 300, "only %zu photons reached the receiver (at least 300 expected)", ph.size());
    for (size_t k = 0; k < radii.size(); k++) {
        size_t within = 0;
        for (const auto &p : ph) {
            if (std::hypot(p.P.x, p.P.y) < radii[k]) {
                within++;
            }
        }
        const double frac = ph.empty() ? 0 : (double)within / (double)ph.size();
        printf("  within %.2f of the axis: %.3f of the photons, %.3f of the exactly traced rays\n", radii[k], frac, expect[k]);
        EXPECT(std::fabs(frac - expect[k]) <= 0.04, "the caustic is not the focus of a ball lens: %.3f of the photons lie within %.2f of the axis, the exact trace gives %.3f",
               frac, radii[k], expect[k]);
    }
    return g_fail ? 1 : 0;
}


// Prism dispersion: the exit sheet of modeExit with `iorrange [1.4 1.8]` and a sampled spectrum. A photon's exit angle gives the index it saw:
// the sheet is tilted by a, the exit angle from the normal is asin(ior sin a), and the stored direction is that angle minus a away from +z. The
// red end of the spectrum (C.r far above C.b) must have seen the first value of iorrange, the blue end the second.
static int modeDispersion() {
    std::vector<Photon> ph;
    const double a = 20 * kPi / 180, t = std::tan(a);
    const std::string face = "-3 -3 " + num(4 + 3 * t) + "  3 -3 " + num(4 - 3 * t) + "  3 3 " + num(4 - 3 * t) + "  -3 3 " + num(4 + 3 * t);
    const std::string model = "Attribute \"photon\" \"string shadingmodel\" [\"dielectric\"] \"float[2] iorrange\" [1.4 1.8]";
    if (!render("refract-plane.rib.in", "dispersion",
                {{"EMIT", "400000"}, {"HIDEREXTRA", "\"int samplespectrum\" [1]"}, {"LIGHT_TO", "0 0 1"}, {"ORIENT", ""}, {"MODEL", model}, {"FACE", face}}, ph)) {
        return 2;
    }
    double sum[2] = {0, 0};
    size_t cnt[2] = {0, 0};
    for (const auto &p : ph) {
        int k = -1;
        if (p.C.x > 2 * p.C.z && p.C.x > 1e-4) {
            k = 0; // red end
        }
        else if (p.C.z > 2 * p.C.x && p.C.z > 1e-4) {
            k = 1; // blue end
        }
        if (k < 0) {
            continue;
        }
        const double fromAxis = std::acos(std::max(-1.0, std::min(1.0, p.dir.z)));
        sum[k] += std::sin(fromAxis + a) / std::sin(a);
        cnt[k]++;
    }
    const double red = cnt[0] ? sum[0] / (double)cnt[0] : 0, blue = cnt[1] ? sum[1] / (double)cnt[1] : 0;
    printf("iorrange [1.4 1.8]: red-end photons (%zu) saw a mean index %.3f, blue-end photons (%zu) %.3f\n", cnt[0], red, cnt[1], blue);
    EXPECT(cnt[0] >= 500 && cnt[1] >= 500, "too few photons at the ends of the spectrum (%zu red, %zu blue; at least 500 each expected)", cnt[0], cnt[1]);
    EXPECT(red >= 1.4 - 0.02 && blue <= 1.8 + 0.02, "an index outside iorrange [1.4 1.8]: red %.3f, blue %.3f", red, blue);
    EXPECT(red + 0.1 <= blue, "the red end saw index %.3f and the blue end %.3f: iorrange is [ior for red, ior for blue], so red must see the lower index here", red, blue);
    return g_fail ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s <entry|exit|focus|dispersion> <glass|water|dielectric> <orender> <scenes dir> <work dir>\n", argv[0]);
        return 2;
    }
    Model m;
    if (!modelFor(argv[2], m)) {
        fprintf(stderr, "unknown model %s\n", argv[2]);
        return 2;
    }
    g_orender = argv[3];
    g_scenes = argv[4];
    g_work = argv[5];
    if (system(("mkdir -p \"" + g_work + "\"").c_str()) != 0) {
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "entry") {
        return modeEntry(m);
    }
    if (mode == "exit") {
        return modeExit(m);
    }
    if (mode == "focus") {
        return modeFocus(m);
    }
    if (mode == "dispersion") {
        return modeDispersion();
    }
    fprintf(stderr, "unknown mode %s\n", mode.c_str());
    return 2;
}
