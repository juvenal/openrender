#include "cameraExport.h"
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {

int ribcam_write(const float *camToWorld16, int projType, float fovDeg,
                 const char *outputPath) {
    CameraExport cam{};
    for (int i = 0; i < 16; i++) cam.cameraToWorld[i] = camToWorld16[i];
    cam.projectionType = projType;
    cam.fov            = fovDeg;
    cam.outputPath     = outputPath;
    cam.updateExisting = false;
    return writeRibCamera(cam) ? 1 : 0;
}

int ribcam_replace(const float *camToWorld16, int projType, float fovDeg,
                   const char *existingPath) {
    CameraExport cam{};
    for (int i = 0; i < 16; i++) cam.cameraToWorld[i] = camToWorld16[i];
    cam.projectionType = projType;
    cam.fov            = fovDeg;
    cam.outputPath     = existingPath;
    cam.updateExisting = true;
    return replaceRibCamera(cam) ? 1 : 0;
}

} // extern "C"

static std::string isoTimestamp() {
    time_t t = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    return buf;
}

bool writeRibCamera(const CameraExport &cam) {
    std::ofstream f(cam.outputPath);
    if (!f) return false;

    f << "## orender-wire camera export\n";
    f << "## Exported: " << isoTimestamp() << "\n\n";

    if (cam.projectionType == 0)
        f << "Projection \"perspective\" \"fov\" [" << cam.fov << "]\n";
    else
        f << "Projection \"orthographic\"\n";

    f << "Transform [";
    for (int i = 0; i < 16; i++) {
        f << cam.cameraToWorld[i];
        if (i < 15) f << " ";
    }
    f << "]\n";

    return f.good();
}

bool replaceRibCamera(const CameraExport &cam) {
    std::ifstream fin(cam.outputPath);
    if (!fin) return false;
    std::string content((std::istreambuf_iterator<char>(fin)), {});
    fin.close();

    size_t worldPos = content.find("WorldBegin");
    if (worldPos == std::string::npos) {
        // No WorldBegin: append new camera before end
        std::ofstream fout(cam.outputPath, std::ios::app);
        return writeRibCamera(cam);
    }

    std::string pre  = content.substr(0, worldPos);
    std::string post = content.substr(worldPos);

    auto replaceLine = [&](const std::string &keyword, const std::string &newLine) {
        size_t pos = pre.find(keyword);
        if (pos == std::string::npos) return;
        size_t end = pre.find('\n', pos);
        pre.replace(pos, end - pos, newLine);
    };

    // Replace or insert Projection
    {
        std::string proj;
        if (cam.projectionType == 0)
            proj = "Projection \"perspective\" \"fov\" [" + std::to_string(cam.fov) + "]";
        else
            proj = "Projection \"orthographic\"";
        replaceLine("Projection", proj);
    }

    // Replace or insert Transform
    {
        std::ostringstream ss;
        ss << "Transform [";
        for (int i = 0; i < 16; i++) { ss << cam.cameraToWorld[i]; if (i<15) ss << " "; }
        ss << "]";
        replaceLine("Transform", ss.str());
    }

    std::ofstream fout(cam.outputPath);
    if (!fout) return false;
    fout << pre << post;
    return fout.good();
}
