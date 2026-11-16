/**
 * tests/shading_system/bench_load.cpp
 *
 * bench_shader_load (spec 033, Phase 1, T018; research R12, P-Q8): the in-process half of the loading measurement. It
 * loads every shipped shader of a directory, as .rslo and as .slo (137 x 2 = 274 objects), through the loader entry
 * points of the starting tree (parseShader(), parseSloShader()) and prints how long the loads took. It is not a ctest
 * test: tools/timing_loading.sh runs it through cpujob, five times, and reports the median and the spread of the last
 * four. In Phase 9 the same program, compiled with OR033_BENCH_USE_SHADING_SYSTEM, loads the same objects through
 * ShadingSystem::find() instead, so the two numbers are comparable.
 *
 * Usage: bench_shader_load <directory holding <name>.rslo and <name>.slo>
 * Output: three lines "objects <n>", "rslo_ms <t>" and "slo_ms <t>" (one pass, one process: the JIT engine's own start-up
 * is part of the first .slo load, as it is for a render).
 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <dirent.h>

#include "config.h"
#include "ri.h"
#include "shader.h"
#include "sloLoader.h"

#ifdef OR033_BENCH_USE_SHADING_SYSTEM
#error "OR033_BENCH_USE_SHADING_SYSTEM: the ShadingSystem loader exists from Phase 4; this branch is written then"
#endif

CShader *parseShader(const char *name, const char *filename);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <shaders directory>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    std::vector<std::string> names;
    if (DIR *d = opendir(dir.c_str())) {
        while (const dirent *e = readdir(d)) {
            const std::string f = e->d_name;
            if (f.size() > 5 && f.compare(f.size() - 5, 5, ".rslo") == 0)
                names.push_back(f.substr(0, f.size() - 5));
        }
        closedir(d);
    }
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        fprintf(stderr, "no .rslo in %s\n", dir.c_str());
        return 2;
    }

    RiBegin(RI_NULL);
    using clock = std::chrono::steady_clock;
    int loaded = 0, failed = 0;

    const auto t0 = clock::now();
    for (const std::string &n : names) {
        CShader *s = parseShader(n.c_str(), (dir + "/" + n + ".rslo").c_str());
        (s != nullptr ? loaded : failed)++;
    }
    const auto t1 = clock::now();
#ifdef OPENRENDER_HAVE_LLVM
    for (const std::string &n : names) {
        CShader *s = parseSloShader(n.c_str(), (dir + "/" + n + ".slo").c_str());
        (s != nullptr ? loaded : failed)++;
    }
#endif
    const auto t2 = clock::now();
    RiEnd();

    if (failed != 0) {
        fprintf(stderr, "%d object(s) failed to load\n", failed);
        return 1;
    }
    printf("objects %d\n", loaded);
    printf("rslo_ms %.3f\n", std::chrono::duration<double, std::milli>(t1 - t0).count());
    printf("slo_ms %.3f\n", std::chrono::duration<double, std::milli>(t2 - t1).count());
    return 0;
}
