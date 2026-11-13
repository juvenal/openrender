// scratch (not tracked): the camera slot draws of every sample of a block of pixels, for the lens investigation
#include <cstdio>
#include <cstdlib>
#include "common/sampling.h"
using namespace openrender::sampling;
int main(int argc, char **argv) {
    const int backendIdx = atoi(argv[1]), n = atoi(argv[2]), pix = atoi(argv[3]);
    CSamplingConfig cfg{};
    cfg.backend = backendIdx == 0 ? Backend::Sobol : backendIdx == 1 ? Backend::PMJ02 : Backend::Independent;
    cfg.seed = 0; cfg.frame = 0; cfg.nx = n; cfg.ny = n; cfg.jitter = 1.0f;
    const CameraSeeds seeds = cameraSeeds(cfg);
    for (int py = 0; py < pix; ++py)
        for (int px = 0; px < pix; ++px)
            for (int row = 0; row < n; ++row)
                for (int col = 0; col < n; ++col) {
                    SampleId id; id.px = px; id.py = py; id.sampleIndex = row * n + col; id.frame = 0; id.seed = 0;
                    const CameraDraws d = cameraDraws(cfg, seeds, id, true);
                    printf("%d %d %d %.7f %.7f %.7f %.7f\n", px, py, row * n + col, d.film.u, d.film.v, d.lens.u, d.lens.v);
                }
    return 0;
}
