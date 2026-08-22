/**
 * tests/visual/test_perf_compare.cpp
 *
 * FR-011/SC-006 manual-only performance comparison for spec
 * 011-jit-opcode-parity: for a given RSL construct, confirms the LLVM JIT
 * (.slo) backend renders no slower than 90% of the .rslo interpreter's
 * wall-clock time.
 *
 * Usage:
 *   test_perf_compare <orender> <rib_rslo> <rib_slo> [max_ratio=0.90] [runs=3]
 *
 * Renders each RIB <runs> times (median wall-clock kept, to reduce
 * machine-load noise per research.md D4) and fails if
 * median(slo_time) > max_ratio * median(rslo_time).
 *
 * Intentionally NOT labelled "visual"/"libshader"/"regression" so it is
 * excluded from this project's default/CI ctest invocations (label
 * "perf-manual" only) -- see research.md D4 and quickstart.md step 4.
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static double renderOnceSeconds(const char *orenderPath, const char *ribPath) {
    std::string cmd = std::string(orenderPath) + " \"" + ribPath + "\" > /dev/null 2>&1";
    auto t0 = std::chrono::steady_clock::now();
    int rc = system(cmd.c_str());
    auto t1 = std::chrono::steady_clock::now();
    if (rc != 0) {
        fprintf(stderr, "  orender failed (exit %d) on %s\n", rc, ribPath);
        return -1.0;
    }
    return std::chrono::duration<double>(t1 - t0).count();
}

static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <orender> <rib_rslo> <rib_slo> [max_ratio=0.90] [runs=3]\n",
            argv[0]);
        return 1;
    }

    const char *orenderPath = argv[1];
    const char *ribRslo     = argv[2];
    const char *ribSlo      = argv[3];
    const double maxRatio   = (argc >= 5) ? atof(argv[4]) : 0.90;
    const int    runs       = (argc >= 6) ? atoi(argv[5]) : 3;

    std::vector<double> rsloTimes, sloTimes;
    for (int i = 0; i < runs; ++i) {
        double t = renderOnceSeconds(orenderPath, ribRslo);
        if (t < 0) return 1;
        rsloTimes.push_back(t);
    }
    for (int i = 0; i < runs; ++i) {
        double t = renderOnceSeconds(orenderPath, ribSlo);
        if (t < 0) return 1;
        sloTimes.push_back(t);
    }

    double rsloMedian = median(rsloTimes);
    double sloMedian  = median(sloTimes);
    double ratio      = (rsloMedian > 0.0) ? (sloMedian / rsloMedian) : 0.0;

    printf("  rslo median: %.4fs  slo median: %.4fs  ratio: %.3f  (max allowed: %.3f)\n",
           rsloMedian, sloMedian, ratio, maxRatio);

    if (rsloMedian < 0.05) {
        printf("  NOTE: rslo median render time is very small (%.4fs) -- process-startup "
               "overhead may dominate; treat this result as a weak signal only.\n", rsloMedian);
    }

    if (ratio > maxRatio) {
        fprintf(stderr, "  FAIL: JIT (slo) render time ratio %.3f exceeds max allowed %.3f\n",
                ratio, maxRatio);
        return 1;
    }

    printf("  PASS: JIT (slo) render time ratio %.3f <= max allowed %.3f\n", ratio, maxRatio);
    return 0;
}
