/**
 * tests/shading_parity/test_derivative_destination.cpp
 *
 * Two defects of the RSL derivative builtins on a REYES grid, found during the GitHub #61
 * investigation (no issue of their own), on both backends:
 *
 *   mask   Du()/Dv() wrote every lane of their destination, ignoring the lane tags. A shader
 *          assigning a derivative inside a varying `if`, `float y = 1; if (s > 0.5) y = Du(f);`,
 *          had y overwritten on the lanes the `if` switched off. The interpreter's DUFEXPR_PRE
 *          and friends called duFloat(res, op) on the destination itself, and the JIT's
 *          op_Du_ff/op_Dv_ff/op_Du_vv/op_Dv_vv handed it straight to the same helpers.
 *   alias  `x = Du(x)` compiles in place (`Du x x`). The grid helpers write lane i before
 *          reading it as the left neighbour of lane i + 1, so the result was garbage.
 *
 * The contract: a derivative builtin leaves the inactive lanes of its destination untouched
 * and computes the same values whether or not the destination is also the source. Deriv(),
 * area() and calculatenormal() already work that way (they take their derivatives into scratch
 * memory and write the destination lane by lane under the mask); they are checked alongside as
 * guards.
 *
 * One fixture is compiled to .rslo and .slo and rendered on a bilinear patch (s = u, t = v,
 * one 9 x 9 grid), and each shading point prints its values. With f = 5 s^2 + t, Du(f) = 10 s
 * and Deriv(f, s) = 10 s exactly under central differencing; with g = 3 t^2 + s, Dv(g) = 6 t.
 *   mask:  on every lane the `if` switched off the destination still holds its value from before
 *          the `if` (1); the active lanes away from the seam and the grid edges hold the
 *          analytic derivative.
 *   alias: every lane away from the grid edges holds the analytic derivative.
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (expr) {                                                            \
            ++g_passed;                                                        \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

static bool writeFile(const std::string &path, const std::string &content) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

static std::string readFile(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return {};
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        s.append(buf, n);
    fclose(f);
    return s;
}

static bool runOshader(const char *oshaderBin, const std::string &srcPath, const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                      "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// orender's exit code is not a useful signal here (see test_printf_jit.cpp): the runs exit
// non-zero on an unrelated "Failed to find shader \"defaultsurface\"" path.
static void runOrender(const char *orenderBin, const char *displaysDir, const std::string &dir, const std::string &ribPath, const std::string &logPath) {
    std::string cmd = std::string("cd \"") + dir + "\" && SHADERS=\"" + dir + "\" ORENDERHOME=\"" + dir +
                      "\" DISPLAYS=\"" + displaysDir + "\" \"" + orenderBin + "\" \"" + ribPath + "\" > \"" +
                      logPath + "\" 2>&1";
    int rc = system(cmd.c_str());
    (void)rc;
}

// The lines that start with `marker`, as rows of numbers
static std::vector<std::vector<double>> rows(const std::string &text, const char *marker) {
    std::vector<std::vector<double>> out;
    std::istringstream in(text);
    std::string line;
    const std::string m = std::string(marker) + " ";
    while (std::getline(in, line)) {
        if (line.compare(0, m.size(), m) != 0)
            continue;
        std::istringstream ls(line.substr(m.size()));
        std::vector<double> r;
        double v;
        while (ls >> v)
            r.push_back(v);
        out.push_back(r);
    }
    return out;
}

// The grid spacing along a parameter: the smallest gap between its distinct printed values
static double spacing(const std::vector<std::vector<double>> &rs, int col) {
    std::set<double> vals;
    for (const auto &r : rs)
        vals.insert(r[col]);
    double best = 1.0;
    for (auto it = vals.begin(), nx = std::next(vals.begin()); vals.size() > 1 && nx != vals.end(); ++it, ++nx)
        best = std::min(best, *nx - *it);
    return best;
}

static bool near(double a, double b) { return fabs(a - b) < 1e-3; }

static const char *kFixture =
    "surface derivative_destination_fixture() {\n"
    "    /* mask: derivatives assigned inside a varying if */\n"
    "    float yf = 1;\n"
    "    if (s > 0.5) yf = Du(s * s * 5 + t);\n"
    "    float zf = 1;\n"
    "    if (t > 0.5) zf = Dv(t * t * 3 + s);\n"
    "    vector yv = vector(1, 1, 1);\n"
    "    if (s > 0.5) yv = Du(vector(s * s * 5, t, s * t));\n"
    "    vector zv = vector(1, 1, 1);\n"
    "    if (t > 0.5) zv = Dv(vector(s, t * t * 3, s * t));\n"
    "    float yd = 1;\n"
    "    if (s > 0.5) yd = Deriv(s * s * 5 + t, s);\n"
    "    float ya = 1;\n"
    "    if (s > 0.5) ya = area(P);\n"
    "    normal yn = normal(1, 1, 1);\n"
    "    if (s > 0.5) yn = calculatenormal(P);\n"
    "    printf(\"M %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\\n\", s, t, yf, zf,\n"
    "           xcomp(yv), ycomp(yv), zcomp(yv), xcomp(zv), ycomp(zv), zcomp(zv), yd, ya,\n"
    "           xcomp(yn), ycomp(yn), zcomp(yn));\n"
    "    /* alias: the destination is the source (compiles to Du x x) */\n"
    "    float xa = s * s * 5 + t;\n"
    "    xa = Du(xa);\n"
    "    float xb = t * t * 3 + s;\n"
    "    xb = Dv(xb);\n"
    "    vector va = vector(s * s * 5, t, s * t);\n"
    "    va = Du(va);\n"
    "    vector vb = vector(s, t * t * 3, s * t);\n"
    "    vb = Dv(vb);\n"
    "    float xd = s * s * 5 + t;\n"
    "    xd = Deriv(xd, s);\n"
    "    printf(\"A %f %f %f %f %f %f %f %f %f %f %f\\n\", s, t, xa, xb,\n"
    "           xcomp(va), ycomp(va), zcomp(va), xcomp(vb), ycomp(vb), zcomp(vb), xd);\n"
    "    Ci = 0;\n"
    "}\n";

static void checkMask(const char *fmt, const std::string &text) {
    const auto rs = rows(text, "M");
    printf("  mask, .%s: %zu shading points\n", fmt, rs.size());
    EXPECT_TRUE(rs.size() >= 25);
    if (rs.empty())
        return;
    const double du = spacing(rs, 0), dv = spacing(rs, 1);
    // Destination columns of the M line, per builtin
    struct Col {
            const char *name;
            std::vector<int> cols;
            bool alongU;
            bool ok;
    };
    std::vector<Col> dests = {{"Du float", {2}, true, true}, {"Dv float", {3}, false, true}, {"Du vector", {4, 5, 6}, true, true}, {"Dv vector", {7, 8, 9}, false, true}, {"Deriv", {10}, true, true}, {"area", {11}, true, true}, {"calculatenormal", {12, 13, 14}, true, true}};
    bool activeU = true, activeV = true, shapeOk = true;
    int nInU = 0, nInV = 0, nActU = 0, nActV = 0;
    for (const auto &r : rs) {
        if (r.size() != 15) {
            shapeOk = false;
            continue;
        }
        const double s = r[0], t = r[1];
        // The lanes the `if (s > 0.5)` switched off keep 1 in every destination
        if (s <= 0.5 + 1e-6) {
            ++nInU;
            for (Col &d : dests)
                if (d.alongU)
                    for (int c : d.cols)
                        d.ok = d.ok && r[c] == 1.0;
        }
        else if (s > 0.5 + du + 1e-6 && s < 1.0 - 1e-6) {
            ++nActU;
            activeU = activeU && near(r[2], 10 * s) && near(r[4], 10 * s) && near(r[5], 0) && near(r[6], t) && near(r[10], 10 * s);
        }
        if (t <= 0.5 + 1e-6) {
            ++nInV;
            for (Col &d : dests)
                if (!d.alongU)
                    for (int c : d.cols)
                        d.ok = d.ok && r[c] == 1.0;
        }
        else if (t > 0.5 + dv + 1e-6 && t < 1.0 - 1e-6) {
            ++nActV;
            activeV = activeV && near(r[3], 6 * t) && near(r[7], 0) && near(r[8], 6 * t) && near(r[9], s);
        }
    }
    printf("    Du/Deriv/area/calculatenormal: %d inactive, %d checked active; Dv: %d inactive, %d checked active\n", nInU, nActU, nInV, nActV);
    EXPECT_TRUE(nInU > 0 && nActU > 0 && nInV > 0 && nActV > 0);
    EXPECT_TRUE(shapeOk);
    for (const Col &d : dests) {
        printf("    %-16s inactive destination lanes %s\n", d.name, d.ok ? "untouched" : "OVERWRITTEN");
        EXPECT_TRUE(d.ok);
    }
    EXPECT_TRUE(activeU);
    EXPECT_TRUE(activeV);
}

static void checkAlias(const char *fmt, const std::string &text) {
    const auto rs = rows(text, "A");
    printf("  alias, .%s: %zu shading points\n", fmt, rs.size());
    EXPECT_TRUE(rs.size() >= 25);
    bool duOk = true, dvOk = true, derivOk = true;
    int nU = 0, nV = 0;
    for (const auto &r : rs) {
        if (r.size() != 11) {
            duOk = false;
            continue;
        }
        const double s = r[0], t = r[1];
        if (s > 1e-6 && s < 1.0 - 1e-6) {
            ++nU;
            duOk = duOk && near(r[2], 10 * s) && near(r[4], 10 * s) && near(r[5], 0) && near(r[6], t);
            derivOk = derivOk && near(r[10], 10 * s);
        }
        if (t > 1e-6 && t < 1.0 - 1e-6) {
            ++nV;
            dvOk = dvOk && near(r[3], 6 * t) && near(r[7], 0) && near(r[8], 6 * t) && near(r[9], s);
        }
    }
    printf("    %d interior lanes along u, %d along v\n", nU, nV);
    EXPECT_TRUE(nU > 0 && nV > 0);
    EXPECT_TRUE(duOk);
    EXPECT_TRUE(dvOk);
    EXPECT_TRUE(derivOk);
}

int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode != "mask" && mode != "alias") {
        fprintf(stderr, "usage: %s mask|alias\n", argv[0]);
        return 2;
    }
    printf("Derivative builtins: %s\n", mode == "mask" ? "inactive destination lanes stay untouched" : "destination aliasing the source");

    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    char tmplBuf[] = "/tmp/shading_parity_derivdest_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return 1;
    const std::string dir = tmpDir;

    const std::string shaderName = "derivative_destination_fixture";
    const std::string src = dir + "/" + shaderName + ".sl";
    EXPECT_TRUE(writeFile(src, kFixture));
    EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + shaderName + ".slo", /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, src, dir + "/" + shaderName + ".rslo", /*jit=*/false));

    for (const char *fmt : {"rslo", "slo"}) {
        const std::string rib = dir + "/scene_" + fmt + ".rib";
        writeFile(rib,
                  "Format 64 64 1\n"
                  "PixelSamples 1 1\n"
                  "ShadingRate 64\n"
                  "Projection \"orthographic\"\n"
                  "ScreenWindow -1 1 -1 1\n"
                  "Hider \"zbuffer\"\n"
                  "Display \"derivdest.tif\" \"file\" \"rgba\"\n"
                  "Translate 0 0 5\n"
                  "WorldBegin\n"
                  "    Attribute \"shade\" \"shaderformat\" [\"" +
                      std::string(fmt) + "\"]\n"
                                         "    Surface \"" +
                      shaderName + "\"\n"
                                   "    Patch \"bilinear\" \"P\" [-1 1 0  1 1 0  -1 -1 0  1 -1 0]\n"
                                   "WorldEnd\n");
        const std::string log = dir + "/out_" + fmt + ".log";
        runOrender(orenderBin, displaysDir, dir, rib, log);
        const std::string text = readFile(log);
        if (mode == "mask")
            checkMask(fmt, text);
        else
            checkAlias(fmt, text);
    }

    // Leave nothing behind in /tmp (GitHub #56)
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
