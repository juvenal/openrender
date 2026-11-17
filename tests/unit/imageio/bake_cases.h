// tests/unit/imageio/bake_cases.h -- the bake cases of spec 048 Phase 6, shared by the legacy-hash tool and the tests.
//
// A case is one `make*` call: a kind (texture, shadow map, cubic, cylinder or spherical environment), its source file(s) and
// whether the OIIO writer takes it or the gate sends it to the legacy writer (data-model section 4, FR-017). The sources are the
// TIFFs of fixtures/bake (written by tools/gen_bake_fixtures.cpp) and, for the four `med_*` cases, the spec 018
// `medium_rgb.tif`, whose four legacy bakes the Phase 1 baseline already pins.
//
// bakeCase() calls the same entry points `otexmake` and the RIB statements use (RiMake*V; the spherical environment, which
// has no RI binding, is called directly), with otexmake's defaults (Catmull-Rom 3x3, resize "up"), inside an RiBegin/RiEnd
// session the caller opens. `imageio` is the per-call "imageio" parameter, or nullptr for none (legacy decode and writer).
#pragma once

#include "ri.h"
#include "texmake.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace bakecases {

    enum class Kind { Texture, Shadow, Cubic, Cylinder, Spherical };

    struct BakeCase {
            const char *name;
            Kind kind;
            const char *source; // under fixtures/bake, or "018:<file>" under the spec 018 fixtures
            bool excluded;      // the gate sends this (bits, channels) to the legacy writer
            int bits;           // 8, 16 or 32 (float)
            int channels;
    };

    inline const std::vector<BakeCase> &cases() {
        static const std::vector<BakeCase> c = {
            // plain textures, 40x24 (not a power of two: the resize path runs)
            {"tex_u8_1", Kind::Texture, "tex40_u8_1.tif", false, 8, 1},
            {"tex_u8_2", Kind::Texture, "tex40_u8_2.tif", false, 8, 2},
            {"tex_u8_3", Kind::Texture, "tex40_u8_3.tif", false, 8, 3},
            {"tex_u8_4", Kind::Texture, "tex40_u8_4.tif", false, 8, 4},
            {"tex_u16_1", Kind::Texture, "tex40_u16_1.tif", false, 16, 1},
            {"tex_u16_2", Kind::Texture, "tex40_u16_2.tif", false, 16, 2},
            {"tex_f32_1", Kind::Texture, "tex40_f32_1.tif", false, 32, 1},
            {"tex_f32_2", Kind::Texture, "tex40_f32_2.tif", false, 32, 2},
            {"tex_f32_3", Kind::Texture, "tex40_f32_3.tif", false, 32, 3},
            {"tex_f32_4", Kind::Texture, "tex40_f32_4.tif", false, 32, 4},
            // a power-of-two source (valid size equals data size: P-Q3a) and sources with a 1-pixel axis
            {"tex_pot64_u8_3", Kind::Texture, "pot64_u8_3.tif", false, 8, 3},
            {"tex_thin_1x16_u8_3", Kind::Texture, "thin_1x16_u8_3.tif", false, 8, 3},
            {"tex_thin_16x1_f32_1", Kind::Texture, "thin_16x1_f32_1.tif", false, 32, 1},
            // the combinations OIIO cannot match: 16-bit with 3 or more channels, 8-bit and float with 5 or more
            {"tex_u16_3", Kind::Texture, "tex40_u16_3.tif", true, 16, 3},
            {"tex_u16_4", Kind::Texture, "tex40_u16_4.tif", true, 16, 4},
            {"tex_u8_5", Kind::Texture, "tex40_u8_5.tif", true, 8, 5},
            {"tex_u8_6", Kind::Texture, "tex40_u8_6.tif", true, 8, 6},
            {"tex_f32_5", Kind::Texture, "tex40_f32_5.tif", true, 32, 5},
            {"tex_f32_6", Kind::Texture, "tex40_f32_6.tif", true, 32, 6},
            // shadow maps: 32x32 sources carrying the two Pixar matrices
            {"shadow_u8_1", Kind::Shadow, "shd_u8_1.tif", false, 8, 1},
            {"shadow_u16_1", Kind::Shadow, "shd_u16_1.tif", false, 16, 1},
            {"shadow_f32_1", Kind::Shadow, "shd_f32_1.tif", false, 32, 1},
            {"shadow_u16_3", Kind::Shadow, "shd_u16_3.tif", true, 16, 3},
            // cylinder (-envlatl), cubic (-envcube: one source six times) and spherical environments, 32x32
            {"cyl_u8_3", Kind::Cylinder, "env_u8_3.tif", false, 8, 3},
            {"cyl_u16_1", Kind::Cylinder, "env_u16_1.tif", false, 16, 1},
            {"cyl_f32_3", Kind::Cylinder, "env_f32_3.tif", false, 32, 3},
            {"cyl_u16_3", Kind::Cylinder, "env_u16_3.tif", true, 16, 3},
            {"cub_u8_3", Kind::Cubic, "env_u8_3.tif", false, 8, 3},
            {"cub_u16_1", Kind::Cubic, "env_u16_1.tif", false, 16, 1},
            {"cub_f32_3", Kind::Cubic, "env_f32_3.tif", false, 32, 3},
            {"cub_u16_3", Kind::Cubic, "env_u16_3.tif", true, 16, 3},
            {"sph_u8_3", Kind::Spherical, "env_u8_3.tif", false, 8, 3},
            {"sph_f32_3", Kind::Spherical, "env_f32_3.tif", false, 32, 3},
            {"sph_u16_3", Kind::Spherical, "env_u16_3.tif", true, 16, 3},
            // the spec 018 medium_rgb.tif in the four CLI modes (the Phase 1 baseline pins these four legacy bakes)
            {"med_texture", Kind::Texture, "018:medium_rgb.tif", false, 8, 3},
            {"med_shadow", Kind::Shadow, "018:medium_rgb.tif", false, 8, 3},
            {"med_envlatl", Kind::Cylinder, "018:medium_rgb.tif", false, 8, 3},
            {"med_envcube", Kind::Cubic, "018:medium_rgb.tif", false, 8, 3},
        };
        return c;
    }

    inline std::string envOr(const char *name, const char *fallback) {
        const char *v = getenv(name);
        return v != nullptr ? v : fallback;
    }

    inline std::string sourcePath(const BakeCase &c) {
        std::string s = c.source;
        if (s.rfind("018:", 0) == 0)
            return envOr("IMAGE_INPUT_FIXTURES_DIR", "tests/unit/image_input/fixtures") + "/" + s.substr(4);
        return envOr("IMAGEIO_FIXTURES_DIR", "tests/unit/imageio/fixtures") + "/bake/" + s;
    }

    // Bakes the case into `out` through the make* entry points. Needs an open RiBegin session.
    inline void bakeCase(const BakeCase &c, const std::string &out, const char *imageio) {
        const std::string src = sourcePath(c);
        const char *resize = "up";
        const char *tokens[2] = {"resize", "imageio"};
        const void *values[2] = {&resize, &imageio};
        const int n = imageio != nullptr ? 2 : 1;
        switch (c.kind) {
        case Kind::Texture:
            RiMakeTextureV(src.c_str(), out.c_str(), (RtToken) "periodic", (RtToken) "periodic", RiCatmullRomFilter, 3, 3, n, (RtToken *)tokens, (RtPointer *)values);
            break;
        case Kind::Shadow:
            RiMakeShadowV(src.c_str(), out.c_str(), n, (RtToken *)tokens, (RtPointer *)values);
            break;
        case Kind::Cylinder:
            RiMakeLatLongEnvironmentV(src.c_str(), out.c_str(), RiCatmullRomFilter, 3, 3, n, (RtToken *)tokens, (RtPointer *)values);
            break;
        case Kind::Cubic:
            RiMakeCubeFaceEnvironmentV(src.c_str(), src.c_str(), src.c_str(), src.c_str(), src.c_str(), src.c_str(), out.c_str(), 90, RiCatmullRomFilter, 3, 3, n,
                                       (RtToken *)tokens, (RtPointer *)values);
            break;
        case Kind::Spherical:
            makeSphericalEnvironment(src.c_str(), out.c_str(), nullptr, RI_PERIODIC, RI_CLAMP, RiCatmullRomFilter, 3, 3, n, tokens, values);
            break;
        }
    }

} // namespace bakecases
