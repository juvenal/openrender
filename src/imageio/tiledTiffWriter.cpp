// tiledTiffWriter.cpp -- orimageio::TiledTiffWriter, the baked-texture container written through OpenImageIO
// (spec 048, T095).
//
// The container is the one the legacy libtiff writer produces (data-model section 4, contracts/bake-writer.md): a
// TIFF of tiled directories, LZW without predictor, orientation top-left, resolution unit none with X and Y
// resolution 1, plus the Pixar tags that Texturing's reader needs. OIIO's TIFF writer sets PHOTOMETRIC and
// EXTRASAMPLES from the channel count and the alpha channel, so the caller hands it only the combinations for which
// those equal the legacy writer's (Texturing's exclusion table).
#include "orImageIO.h"

#include "common/diagnostics.h"

#include <OpenImageIO/imageio.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <vector>

namespace orimageio {

    namespace {

        size_t sampleBytes(SampleType t) {
            switch (t) {
            case SampleType::UInt8:
                return 1;
            case SampleType::UInt16:
            case SampleType::Half:
                return 2;
            case SampleType::Float:
                return 4;
            }
            return 0;
        }

        OIIO::TypeDesc oiioType(SampleType t) {
            switch (t) {
            case SampleType::UInt8:
                return OIIO::TypeUInt8;
            case SampleType::UInt16:
                return OIIO::TypeUInt16;
            case SampleType::Half:
                return OIIO::TypeHalf;
            case SampleType::Float:
                return OIIO::TypeFloat;
            }
            return OIIO::TypeUnknown;
        }

        // PIXAR_IMAGEFULLWIDTH/LENGTH are written by OIIO only when the display window differs from the data window or
        // has an offset, and as (full + offset). A baked pyramid always carries them, even when equal to the data size
        // (P-Q3a, research R8), so the valid size is expressed as a display window with an offset of 1 on x, and on y
        // as well when the valid length is at least 2: the value written is then the valid size. The offset on x keeps
        // the window "different" for any size, and the y axis stays plain when its valid length is 1 so that no
        // window is ever empty on both axes.
        void setFullSize(OIIO::ImageSpec &spec, const DirectoryDesc &d) {
            if (d.fullWidth <= 0 || d.fullHeight <= 0)
                return;
            spec.full_x = 1;
            spec.full_width = d.fullWidth - 1;
            if (d.fullHeight >= 2) {
                spec.full_y = 1;
                spec.full_height = d.fullHeight - 1;
            }
            else {
                spec.full_y = 0;
                spec.full_height = d.fullHeight;
            }
        }

    } // namespace

    struct TiledTiffWriter::Impl {
            std::string path;
            std::unique_ptr<OIIO::ImageOutput> out;
            int directories = 0;
            bool started = false;      // OIIO opened the file: it is ours to remove on failure
            bool failed = false;
            bool closed = false;
    };

    TiledTiffWriter::TiledTiffWriter(const char *path) : impl(new Impl) {
        (void)oiioVersion(); // the one-time OIIO setup
        impl->path = path;
    }

    TiledTiffWriter::~TiledTiffWriter() {
        close();
        delete impl;
    }

    bool TiledTiffWriter::appendDirectory(const DirectoryDesc &d, const void *pixels) {
        if (impl->failed || impl->closed)
            return false;
        try {
            if (impl->out == nullptr) {
                impl->out = OIIO::ImageOutput::create("tiff");
                if (impl->out == nullptr) {
                    error(CODE_SYSTEM, "OpenImageIO has no writer for the tiff format (\"%s\"): %s\n", impl->path.c_str(), OIIO::geterror().c_str());
                    impl->failed = true;
                    return false;
                }
            }

            const OIIO::TypeDesc type = oiioType(d.type);
            OIIO::ImageSpec spec(d.width, d.height, d.channels, type);
            spec.tile_width = d.tileSize;
            spec.tile_height = d.tileSize;
            spec.tile_depth = 1;
            setFullSize(spec, d);

            // The channels and the alpha: legacy writes one UNASSALPHA extra sample for 2 and 4 channels (8-bit and
            // float), none for a 16-bit directory (which has no PHOTOMETRIC_RGB either).
            static const char *const names1[] = {"Y"};
            static const char *const names2[] = {"Y", "A"};
            static const char *const names3[] = {"R", "G", "B"};
            static const char *const names4[] = {"R", "G", "B", "A"};
            const char *const *names = d.channels == 1 ? names1 : d.channels == 2 ? names2 : d.channels == 3 ? names3 : names4;
            spec.channelnames.assign(names, names + std::min(d.channels, 4));
            const bool alpha = (d.channels == 2 || d.channels == 4) && d.type != SampleType::UInt16;
            spec.alpha_channel = alpha ? d.channels - 1 : -1;
            if (d.type == SampleType::UInt16)
                spec.attribute("tiff:write_extrasamples", 0);
            spec["oiio:UnassociatedAlpha"] = 1;

            // OIIO writes SOFTWARE only when given one; naming the writer lets a reader of the file tell who baked it
            spec.attribute("Software", std::string("OpenImageIO ") + oiioVersion());

            spec.attribute("compression", "lzw");
            spec.attribute("tiff:Predictor", 1);
            spec.attribute("Orientation", 1);
            spec.attribute("ResolutionUnit", "none");
            spec.attribute("XResolution", 1.0f);
            spec.attribute("YResolution", 1.0f);

            if (d.textureFormat != nullptr)
                spec.attribute("textureformat", d.textureFormat);
            if (d.wrapModes != nullptr)
                spec.attribute("wrapmodes", d.wrapModes);
            if (d.worldToCamera != nullptr)
                spec.attribute("worldtocamera", OIIO::TypeMatrix, d.worldToCamera);
            if (d.worldToScreen != nullptr)
                spec.attribute("worldtoscreen", OIIO::TypeMatrix, d.worldToScreen);

            const OIIO::ImageOutput::OpenMode mode = impl->directories == 0 ? OIIO::ImageOutput::Create : OIIO::ImageOutput::AppendSubimage;
            if (!impl->out->open(impl->path, spec, mode)) {
                error(CODE_SYSTEM, "Failed to create \"%s\" for writing: %s\n", impl->path.c_str(), impl->out->geterror().c_str());
                impl->failed = true;
                return false;
            }
            impl->started = true;

            // One tile at a time, as the legacy writer does; a tile that runs past the data is padded with zeros.
            const size_t pixelSize = sampleBytes(d.type) * (size_t)d.channels;
            const size_t tileRow = pixelSize * (size_t)d.tileSize;
            std::vector<unsigned char> tile(tileRow * (size_t)d.tileSize);
            const unsigned char *src = static_cast<const unsigned char *>(pixels);
            for (int y = 0; y < d.height; y += d.tileSize) {
                for (int x = 0; x < d.width; x += d.tileSize) {
                    std::fill(tile.begin(), tile.end(), 0);
                    const int w = std::min(d.tileSize, d.width - x);
                    for (int ty = 0; ty < d.tileSize && y + ty < d.height; ty++)
                        std::memcpy(&tile[ty * tileRow], src + ((size_t)(y + ty) * d.width + x) * pixelSize, (size_t)w * pixelSize);
                    if (!impl->out->write_tile(x, y, 0, type, tile.data())) {
                        error(CODE_SYSTEM, "Failed to write \"%s\": %s\n", impl->path.c_str(), impl->out->geterror().c_str());
                        impl->failed = true;
                        return false;
                    }
                }
            }
            impl->directories++;
            return true;
        }
        catch (const std::exception &e) {
            error(CODE_SYSTEM, "Failed to write \"%s\": %s\n", impl->path.c_str(), e.what());
        }
        catch (...) {
            error(CODE_SYSTEM, "Failed to write \"%s\": unknown exception\n", impl->path.c_str());
        }
        impl->failed = true;
        return false;
    }

    bool TiledTiffWriter::close() {
        if (impl->closed)
            return !impl->failed;
        impl->closed = true;
        try {
            if (impl->out != nullptr) {
                if (!impl->out->close()) {
                    error(CODE_SYSTEM, "Failed to finish \"%s\": %s\n", impl->path.c_str(), impl->out->geterror().c_str());
                    impl->failed = true;
                }
                impl->out.reset();
            }
        }
        catch (...) {
            impl->failed = true;
        }
        if (impl->failed && impl->started)
            std::remove(impl->path.c_str());
        return !impl->failed;
    }

} // namespace orimageio
