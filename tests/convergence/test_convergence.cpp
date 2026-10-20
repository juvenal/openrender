/**
 * tests/convergence/test_convergence.cpp
 *
 * Convergence harness (spec 039, FR-033, SC-003 part b).
 *
 * For one scene template it renders a ladder of sample counts with each sampler backend, computes the
 * RMSE of every render against a high-sample-count reference over all channels, fits the log-log slope
 * of RMSE against the sample count by least squares, prints the scene x sampler x rung table, and
 * checks the SC-003 bar of the default backend against the independent backend.
 *
 *   test_convergence --orender <path> --template <rib.in> --reference <ref.tif>
 *                    --knob spp|samples [--ladder 1,4,16,64,256] [--samplers sobol,pmj02,independent]
 *                    [--bar render|nonworse|none] [--seeds 8] [--output convergence-out.tif]
 *                    [--default sobol] [--baseline independent] [--scratch <dir>] [--margin 0.15]
 *
 *   test_convergence --make-reference --orender <path> --template <rib.in> --reference <out.tif>
 *                    --knob spp|samples --ref-knob 256 --ref-renders 16 [--ref-sampler independent]
 *
 * The template is a RIB with these placeholders, replaced before each render:
 *   @SAMPLER@  the backend name (sobol, pmj02, independent)
 *   @SEED@     the sampling seed (a different one for every render, so the RMSE is over seeds)
 *   @SPP@      with --knob spp: "nx ny", the PixelSamples of a rung (nx * ny = the rung, e.g. 16 -> "4 4")
 *   @SAMPLES@  with --knob samples: the rung, the sample count of the shading-side integral
 * The scene's Display must write --output (default convergence-out.tif) as 32-bit float in the working
 * directory; the harness runs every render in --scratch.
 *
 * The reference is the mean of --ref-renders renders at the knob value --ref-knob with the independent
 * backend and seeds that no rung uses, so its noise is that of --ref-knob * --ref-renders independent
 * samples (research M4).
 *
 * Bars (SC-003). "Slope" is the fitted exponent of RMSE against the rung, so steeper is more negative.
 *   render:   slope(default) <= slope(baseline) - margin  AND  rmse_top(default) <= rmse_top(baseline)
 *   nonworse: slope(default) <= slope(baseline)           AND  rmse_top(default) <= rmse_top(baseline)
 * Exit codes: 0 every bar met, 1 a bar failed, 2 a render, decode or usage failure.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../visual/depth_tiff.h"

namespace {

    constexpr int kExitBar = 1;
    constexpr int kExitFailure = 2;

    struct Args {
            std::string orender, tmpl, reference, knob = "spp", bar = "none", output = "convergence-out.tif";
            std::string scratch = ".", defaultSampler = "sobol", baseline = "independent", refSampler = "independent";
            std::vector<int> ladder = {1, 4, 16, 64, 256};
            std::vector<std::string> samplers = {"sobol", "pmj02", "independent"};
            int seeds = 8, refKnob = 256, refRenders = 16;
            double margin = 0.15;
            bool makeReference = false;
    };

    std::vector<std::string> split(const std::string &s, char sep) {
        std::vector<std::string> parts;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, sep))
            if (!item.empty())
                parts.push_back(item);
        return parts;
    }

    bool parseArgs(int argc, char **argv, Args &a) {
        for (int i = 1; i < argc; i++) {
            const std::string k = argv[i];
            if (k == "--make-reference") {
                a.makeReference = true;
                continue;
            }
            if (i + 1 >= argc) {
                fprintf(stderr, "  %s expects a value\n", k.c_str());
                return false;
            }
            const char *v = argv[++i];
            if (k == "--orender")
                a.orender = v;
            else if (k == "--template")
                a.tmpl = v;
            else if (k == "--reference")
                a.reference = v;
            else if (k == "--knob")
                a.knob = v;
            else if (k == "--bar")
                a.bar = v;
            else if (k == "--output")
                a.output = v;
            else if (k == "--scratch")
                a.scratch = v;
            else if (k == "--default")
                a.defaultSampler = v;
            else if (k == "--baseline")
                a.baseline = v;
            else if (k == "--ref-sampler")
                a.refSampler = v;
            else if (k == "--seeds")
                a.seeds = atoi(v);
            else if (k == "--ref-knob")
                a.refKnob = atoi(v);
            else if (k == "--ref-renders")
                a.refRenders = atoi(v);
            else if (k == "--margin")
                a.margin = atof(v);
            else if (k == "--samplers")
                a.samplers = split(v, ',');
            else if (k == "--ladder") {
                a.ladder.clear();
                for (const std::string &s : split(v, ','))
                    a.ladder.push_back(atoi(s.c_str()));
            }
            else {
                fprintf(stderr, "  Unknown option %s\n", k.c_str());
                return false;
            }
        }
        if (a.orender.empty() || a.tmpl.empty() || a.reference.empty() || (a.knob != "spp" && a.knob != "samples") || a.ladder.empty() ||
            a.seeds < 1 || a.refKnob < 1 || a.refRenders < 1 || (a.bar != "render" && a.bar != "nonworse" && a.bar != "none")) {
            fprintf(stderr, "Usage: %s --orender <path> --template <rib.in> --reference <ref.tif> --knob spp|samples [options]\n", argv[0]);
            return false;
        }
        return true;
    }

    std::string readFile(const std::string &path) {
        std::ifstream in(path);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void replaceAll(std::string &s, const std::string &from, const std::string &to) {
        for (size_t at = 0; (at = s.find(from, at)) != std::string::npos; at += to.size())
            s.replace(at, from.size(), to);
    }

    // The PixelSamples of a rung of n samples per pixel: nx * ny = n, as square as it gets
    std::string pixelSamples(int n) {
        int nx = (int)std::lround(std::sqrt((double)n));
        while (nx > 1 && n % nx != 0)
            nx--;
        return std::to_string(n / nx) + " " + std::to_string(nx);
    }

    // Renders the template once; the image is left in `img`
    bool render(const Args &a, const std::string &tmpl, const std::string &sampler, int seed, int rung, DepthImage &img) {
        std::string rib = tmpl;
        replaceAll(rib, "@SAMPLER@", sampler);
        replaceAll(rib, "@SEED@", std::to_string(seed));
        replaceAll(rib, "@SPP@", pixelSamples(rung));
        replaceAll(rib, "@SAMPLES@", std::to_string(rung));
        const std::string ribPath = (std::filesystem::path(a.scratch) / "convergence-rung.rib").string();
        {
            std::ofstream out(ribPath, std::ios::binary | std::ios::trunc);
            out << rib;
            if (!out) {
                fprintf(stderr, "  Cannot write %s\n", ribPath.c_str());
                return false;
            }
        }
        const std::string outPath = (std::filesystem::path(a.scratch) / a.output).string();
        remove(outPath.c_str());
        const std::string command = "cd \"" + a.scratch + "\" && \"" + a.orender + "\" convergence-rung.rib > convergence-orender.log 2>&1";
        if (system(command.c_str()) != 0) {
            fprintf(stderr, "  orender failed (sampler %s, seed %d, rung %d); see %s/convergence-orender.log\n", sampler.c_str(), seed, rung,
                    a.scratch.c_str());
            return false;
        }
        return readFloatTiff(outPath.c_str(), img);
    }

    bool writeFloatTiff(const char *path, const DepthImage &img) {
        TIFF *tif = TIFFOpen(path, "w");
        if (!tif) {
            fprintf(stderr, "  Cannot write %s\n", path);
            return false;
        }
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, img.width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, img.height);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, (uint16_t)img.channels);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, (uint16_t)32);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, (uint16_t)SAMPLEFORMAT_IEEEFP);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, (uint16_t)PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, (uint16_t)(img.channels >= 3 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK));
        if (img.channels == 4) {
            const uint16_t extra = EXTRASAMPLE_UNASSALPHA;
            TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, &extra);
        }
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, img.height);
        const size_t rowValues = (size_t)img.width * img.channels;
        for (uint32_t row = 0; row < img.height; row++) {
            if (TIFFWriteScanline(tif, const_cast<float *>(&img.z[row * rowValues]), row) < 0) {
                TIFFClose(tif);
                return false;
            }
        }
        TIFFClose(tif);
        return true;
    }

    double rmse(const DepthImage &a, const DepthImage &b) {
        double sum = 0;
        for (size_t i = 0; i < a.z.size(); i++) {
            const double d = (double)a.z[i] - (double)b.z[i];
            sum += d * d;
        }
        return std::sqrt(sum / (double)a.z.size());
    }

    // Least-squares slope of log(rmse) against log(n)
    double fitSlope(const std::vector<int> &ns, const std::vector<double> &errs) {
        std::vector<double> lx, ly;
        for (size_t i = 0; i < ns.size(); i++) {
            if (errs[i] > 0) {
                lx.push_back(std::log((double)ns[i]));
                ly.push_back(std::log(errs[i]));
            }
        }
        if (lx.size() < 2)
            return 0;
        double mx = 0, my = 0;
        for (size_t i = 0; i < lx.size(); i++) {
            mx += lx[i] / lx.size();
            my += ly[i] / lx.size();
        }
        double sxy = 0, sxx = 0;
        for (size_t i = 0; i < lx.size(); i++) {
            sxy += (lx[i] - mx) * (ly[i] - my);
            sxx += (lx[i] - mx) * (lx[i] - mx);
        }
        return sxx > 0 ? sxy / sxx : 0;
    }

    int makeReference(const Args &a, const std::string &tmpl) {
        DepthImage sum;
        for (int r = 0; r < a.refRenders; r++) {
            DepthImage img;
            // Seeds from 100000 up: no rung of the ladder uses them
            if (!render(a, tmpl, a.refSampler, 100000 + r, a.refKnob, img))
                return kExitFailure;
            if (r == 0) {
                sum = img;
                for (float &v : sum.z)
                    v = 0;
            }
            if (img.width != sum.width || img.height != sum.height || img.channels != sum.channels) {
                fprintf(stderr, "  render %d has another size\n", r);
                return kExitFailure;
            }
            for (size_t i = 0; i < img.z.size(); i++)
                sum.z[i] += img.z[i];
            printf("  reference render %d of %d done\n", r + 1, a.refRenders);
            fflush(stdout);
        }
        for (float &v : sum.z)
            v /= (float)a.refRenders;
        if (!writeFloatTiff(a.reference.c_str(), sum))
            return kExitFailure;
        printf("Reference %s: %ux%u, mean of %d renders at %d %s with the %s sampler (%d independent samples per pixel)\n", a.reference.c_str(),
               sum.width, sum.height, a.refRenders, a.refKnob, a.knob.c_str(), a.refSampler.c_str(), a.refKnob * a.refRenders);
        return 0;
    }

} // namespace

int main(int argc, char **argv) {
    Args a;
    if (!parseArgs(argc, argv, a))
        return kExitFailure;
    const std::string tmpl = readFile(a.tmpl);
    if (tmpl.empty()) {
        fprintf(stderr, "  Cannot read template %s\n", a.tmpl.c_str());
        return kExitFailure;
    }
    std::filesystem::create_directories(a.scratch);

    if (a.makeReference)
        return makeReference(a, tmpl);

    DepthImage reference;
    if (!readFloatTiff(a.reference.c_str(), reference))
        return kExitFailure;

    struct Row {
            std::vector<double> rmse;
            double slope = 0;
    };
    std::map<std::string, Row> rows;
    for (const std::string &sampler : a.samplers) {
        Row row;
        for (int rung : a.ladder) {
            // The RMSE of a rung is over the seeds: sqrt of the mean of the per-render mean squared errors
            double mse = 0;
            for (int s = 0; s < a.seeds; s++) {
                DepthImage img;
                if (!render(a, tmpl, sampler, 1 + s, rung, img))
                    return kExitFailure;
                if (img.width != reference.width || img.height != reference.height || img.channels != reference.channels) {
                    fprintf(stderr, "  render is %ux%ux%u, the reference %ux%ux%u\n", img.width, img.height, img.channels, reference.width,
                            reference.height, reference.channels);
                    return kExitFailure;
                }
                const double e = rmse(img, reference);
                mse += e * e / a.seeds;
            }
            row.rmse.push_back(std::sqrt(mse));
        }
        row.slope = fitSlope(a.ladder, row.rmse);
        rows[sampler] = row;
    }

    printf("Convergence: %s, knob %s, %d seed(s) per rung, reference %s\n", a.tmpl.c_str(), a.knob.c_str(), a.seeds, a.reference.c_str());
    printf("%-12s", "sampler");
    for (int rung : a.ladder)
        printf(" %12s", ("n=" + std::to_string(rung)).c_str());
    printf(" %8s\n", "slope");
    for (const std::string &sampler : a.samplers) {
        printf("%-12s", sampler.c_str());
        for (double e : rows[sampler].rmse)
            printf(" %12.6g", e);
        printf(" %8.3f\n", rows[sampler].slope);
    }

    if (a.bar == "none")
        return 0;
    if (!rows.count(a.defaultSampler) || !rows.count(a.baseline)) {
        fprintf(stderr, "  the bar needs the %s and %s samplers in --samplers\n", a.defaultSampler.c_str(), a.baseline.c_str());
        return kExitFailure;
    }
    const Row &d = rows[a.defaultSampler], &b = rows[a.baseline];
    const double need = a.bar == "render" ? a.margin : 0.0;
    const bool slopeOk = d.slope <= b.slope - need;
    const bool topOk = d.rmse.back() <= b.rmse.back();
    printf("Bar %s: slope %s %.3f against %s %.3f (needs to be at least %.2f steeper: %s); top-rung RMSE %.6g against %.6g (%s)\n", a.bar.c_str(),
           a.defaultSampler.c_str(), d.slope, a.baseline.c_str(), b.slope, need, slopeOk ? "ok" : "FAIL", d.rmse.back(), b.rmse.back(),
           topOk ? "ok" : "FAIL");
    if (slopeOk && topOk) {
        printf("  PASS\n");
        return 0;
    }
    printf("  FAIL\n");
    return kExitBar;
}
