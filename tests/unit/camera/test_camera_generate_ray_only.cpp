/**
 * Project: openRender
 *
 * File: test_camera_generate_ray_only.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-009 / FR-010 / SC-010 / US5 AS2-AS3:
 *   Camera_GenerateRayOnly. A State-only program. A camera that implements ONLY
 *   generateRay is a valid ICamera, which is the room the interface reserves for a
 *   bidirectional integrator without forcing it on any implementer:
 *     - the test double compiles against ICamera and runs;
 *     - sampleRay(CameraSample) forwards to generateRay with the same arguments;
 *     - lensModel() defaults to a pinhole;
 *     - evaluateImportance() and sampleConnection() return CameraSupport::NotSupported
 *       and leave their output untouched. The two output types are only forward
 *       declared by camera.h (the BDPT spec defines them), so the test passes opaque,
 *       sentinel-filled storage through a reinterpret_cast, as the contract allows.
 *
 *   Red on the starting tree by build failure: camera.h does not exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdint>
#include <type_traits>

#include "camera.h"
#include "frame_test_utils.h"

using openrender::CameraConnectionSample;
using openrender::CameraImportance;
using openrender::CameraRay;
using openrender::CameraSample;
using openrender::CameraSupport;
using openrender::ICamera;
using openrender::LensModel;

namespace {

    // Implements only generateRay. Its answer encodes the arguments so a forwarder is observable.
    class COnlyGenerateRay final : public ICamera {
        public:
            mutable int calls = 0;

            CameraRay generateRay(float x, float y, float t, const float lensUV[2]) const override {
                ++calls;
                CameraRay r;
                r.from[0] = x;
                r.from[1] = y;
                r.from[2] = t;
                r.dir[0] = lensUV[0];
                r.dir[1] = lensUV[1];
                r.dir[2] = 1.0f;
                r.time = t;
                r.da = 7.0f;
                r.db = 9.0f;
                return r;
            }
    };

    static_assert(std::is_abstract_v<ICamera>, "ICamera has the pure virtual generateRay");
    static_assert(!std::is_abstract_v<COnlyGenerateRay>, "generateRay is the only operation an implementer must provide");
    static_assert(std::is_polymorphic_v<ICamera> && std::has_virtual_destructor_v<ICamera>, "ICamera is an interface");

    constexpr unsigned char kSentinel = 0xA5;

    template <class T>
    bool untouched(const unsigned char *storage, size_t n) {
        (void)sizeof(T *); // the type stays incomplete; only its address is used
        for (size_t i = 0; i < n; ++i)
            if (storage[i] != kSentinel)
                return false;
        return true;
    }

} // namespace

int main() {
    COnlyGenerateRay camera;
    const ICamera &icam = camera;

    // sampleRay forwards to generateRay with the same arguments
    const CameraSample s{12.5f, 99.0f, 0.375f, -0.25f, 0.5f};
    const float lens[2] = {s.lensU, s.lensV};
    const CameraRay direct = icam.generateRay(s.x, s.y, s.time, lens);
    const int before = camera.calls;
    const CameraRay viaSample = icam.sampleRay(s);
    CHECK(camera.calls == before + 1);
    CHECK(frametest::sameBits(direct.from, viaSample.from, 3));
    CHECK(frametest::sameBits(direct.dir, viaSample.dir, 3));
    CHECK(frametest::sameBits(direct.da, viaSample.da) && frametest::sameBits(direct.db, viaSample.db) && frametest::sameBits(direct.time, viaSample.time));
    CHECK(viaSample.from[0] == 12.5f && viaSample.from[1] == 99.0f && viaSample.from[2] == 0.375f);
    CHECK(viaSample.dir[0] == -0.25f && viaSample.dir[1] == 0.5f);

    // lensModel() is a pinhole by default
    const LensModel lm = icam.lensModel();
    CHECK(lm.aperture == 0.0f);
    CHECK(!lm.thinLens());
    CHECK(lm.cocFactorScreen() == 0.0f);

    // The reserved operations: NotSupported, outputs untouched
    alignas(16) unsigned char importance[256];
    alignas(16) unsigned char connection[256];
    memset(importance, kSentinel, sizeof(importance));
    memset(connection, kSentinel, sizeof(connection));
    const float ref[3] = {1.0f, 2.0f, 3.0f};
    CHECK(icam.evaluateImportance(direct, *reinterpret_cast<CameraImportance *>(importance)) == CameraSupport::NotSupported);
    CHECK(icam.sampleConnection(ref, s, *reinterpret_cast<CameraConnectionSample *>(connection)) == CameraSupport::NotSupported);
    CHECK(untouched<CameraImportance>(importance, sizeof(importance)));
    CHECK(untouched<CameraConnectionSample>(connection, sizeof(connection)));

    return TEST_RESULT();
}
