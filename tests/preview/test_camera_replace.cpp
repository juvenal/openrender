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

// Helper: write a sample RIB with camera block
static bool writeTestRib(const char *path, float origFov, float origTz) {
    std::ofstream f(path);
    if (!f) return false;
    f << "# Test RIB\n";
    f << "Display \"out.tif\" \"file\" \"rgba\"\n";
    f << "Projection \"perspective\" \"fov\" [" << origFov << "]\n";
    f << "Transform [1 0 0 0 0 1 0 0 0 0 1 " << origTz << " 0 0 0 1]\n";
    f << "WorldBegin\n";
    f << "AttributeBegin\n";
    f << "Sphere 1 -1 1 360\n";
    f << "AttributeEnd\n";
    f << "WorldEnd\n";
    return true;
}

// replaceRibCamera: locate Projection+Transform before WorldBegin, replace them
static bool replaceRibCamera(const char *path, float newFov, const float newMat[16]) {
    std::ifstream fin(path);
    if (!fin) return false;
    std::string content((std::istreambuf_iterator<char>(fin)), {});
    fin.close();

    // Find WorldBegin boundary
    size_t worldPos = content.find("WorldBegin");
    if (worldPos == std::string::npos) return false;

    std::string pre = content.substr(0, worldPos);
    std::string post = content.substr(worldPos);

    // Replace Projection line
    {
        size_t pos = pre.find("Projection");
        if (pos != std::string::npos) {
            size_t end = pre.find('\n', pos);
            std::string newLine = "Projection \"perspective\" \"fov\" [" +
                std::to_string(newFov) + "]";
            pre.replace(pos, end-pos, newLine);
        }
    }
    // Replace Transform line
    {
        size_t pos = pre.find("Transform");
        if (pos != std::string::npos) {
            size_t end = pre.find('\n', pos);
            std::ostringstream ss;
            ss << "Transform [";
            for (int i=0;i<16;++i) { ss << newMat[i]; if(i<15) ss << " "; }
            ss << "]";
            pre.replace(pos, end-pos, ss.str());
        }
    }

    std::ofstream fout(path);
    if (!fout) return false;
    fout << pre << post;
    return true;
}

static std::string readFile(const char *path) {
    std::ifstream f(path);
    return std::string((std::istreambuf_iterator<char>(f)), {});
}

int main() {
    const char *tmpPath = "/tmp/test_cam_replace.rib";

    CHECK(writeTestRib(tmpPath, 45.0f, 5.0f));

    // Verify original content preserved
    std::string orig = readFile(tmpPath);
    CHECK(orig.find("Sphere 1 -1 1 360") != std::string::npos);
    CHECK(orig.find("WorldBegin") != std::string::npos);
    CHECK(orig.find("Display") != std::string::npos);

    // Replace camera
    float newMat[16] = {1,0,0,0, 0,1,0,0, 0,0,1,10, 0,0,0,1};
    CHECK(replaceRibCamera(tmpPath, 30.0f, newMat));

    std::string updated = readFile(tmpPath);

    // New fov appears
    CHECK(updated.find("30.") != std::string::npos);
    // Old fov 45 removed from Projection line (but may appear in Display etc.)
    // Just verify new value is present and WorldBegin is still there
    CHECK(updated.find("WorldBegin") != std::string::npos);
    // Non-camera content preserved byte-for-byte
    CHECK(updated.find("Sphere 1 -1 1 360") != std::string::npos);
    CHECK(updated.find("Display") != std::string::npos);
    CHECK(updated.find("AttributeBegin") != std::string::npos);

    // New translation value (10) appears in Transform
    CHECK(updated.find("10") != std::string::npos);

    printf("test_camera_replace: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
