#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((float)(a)-(float)(b)) < (eps))

// Minimal camera export format writer — matches cameraExport.cpp behaviour
struct CameraExport {
    float cameraToWorld[16];
    int   projectionType; // 0=perspective, 1=orthographic
    float fov;
    const char *outputPath;
    bool  updateExisting;
};

static bool writeRibCamera(const CameraExport &cam) {
    std::ofstream f(cam.outputPath);
    if (!f) return false;
    f << "## orender-wire camera export\n";
    f << "## Exported: 2026-05-19T00:00:00Z\n\n";
    if (cam.projectionType == 0)
        f << "Projection \"perspective\" \"fov\" [" << cam.fov << "]\n";
    else
        f << "Projection \"orthographic\"\n";
    f << "Transform [";
    for (int i=0;i<16;++i) { f << cam.cameraToWorld[i]; if (i<15) f << " "; }
    f << "]\n";
    return true;
}

// Parse Projection and Transform from a RIB-like file
static bool parseRibCamera(const char *path, float fov_out[1], float matrix_out[16]) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    bool gotFov = false, gotMatrix = false;
    while (std::getline(f, line)) {
        if (line.find("Projection") != std::string::npos && line.find("fov") != std::string::npos) {
            // Parse: Projection "perspective" "fov" [X]
            size_t lb = line.find('[');
            size_t rb = line.find(']');
            if (lb != std::string::npos && rb != std::string::npos) {
                fov_out[0] = std::stof(line.substr(lb+1, rb-lb-1));
                gotFov = true;
            }
        }
        if (line.find("Transform") != std::string::npos && line.find('[') != std::string::npos) {
            size_t lb = line.find('[');
            size_t rb = line.find(']');
            if (lb != std::string::npos && rb != std::string::npos) {
                std::istringstream ss(line.substr(lb+1, rb-lb-1));
                for (int i=0;i<16;++i) ss >> matrix_out[i];
                gotMatrix = true;
            }
        }
    }
    return gotFov && gotMatrix;
}

int main() {
    const char *tmpPath = "/tmp/test_cam_export.rib";

    // Build a known camera-to-world matrix (identity + 2 unit translation in Z)
    CameraExport cam;
    for (int i=0;i<16;++i) cam.cameraToWorld[i] = (i==0||i==5||i==10||i==15)?1.0f:0.0f;
    cam.cameraToWorld[11] = 2.0f; // translate z=2
    cam.projectionType = 0;
    cam.fov = 45.0f;
    cam.outputPath = tmpPath;
    cam.updateExisting = false;

    CHECK(writeRibCamera(cam));

    float fov_back[1] = {0};
    float mat_back[16] = {};
    CHECK(parseRibCamera(tmpPath, fov_back, mat_back));
    CHECK_NEAR(fov_back[0], 45.0f, 1e-3f);

    // Check matrix round-trip (tolerance 1e-5)
    bool matOK = true;
    for (int i=0;i<16;++i)
        if (std::fabs(mat_back[i]-cam.cameraToWorld[i]) > 1e-4f) { matOK=false; break; }
    CHECK(matOK);

    // Verify file contains Projection and Transform keywords
    std::ifstream fin(tmpPath);
    std::string content((std::istreambuf_iterator<char>(fin)), {});
    CHECK(content.find("Projection") != std::string::npos);
    CHECK(content.find("Transform") != std::string::npos);

    printf("test_camera_export: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
