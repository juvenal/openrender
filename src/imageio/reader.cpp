// reader.cpp -- orimageio::Reader, the one-shot decode of a source image through OpenImageIO (spec 048, T063).
//
// What it neutralises (research R6): OIIO's PNG and TIFF readers associate alpha and convert some colour models
// unless told not to, so every open passes oiio:UnassociatedAlpha=1 and oiio:RawColor=1; the static
// ImageInput::open(filename) probes other formats when the suffix's reader fails, so the reader is created by
// format name and the format it reports is checked afterwards; and no ColorConfig conversion API is ever called.
#include "orImageIO.h"

#include "common/diagnostics.h"

#include <OpenImageIO/imageio.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>

namespace orimageio {

    namespace {

        // OIIO's name of each format's reader
        const char *formatName(Format f) {
            switch (f) {
            case Format::Tiff:
                return "tiff";
            case Format::Png:
                return "png";
            case Format::Rgbe:
                return "hdr";
            case Format::OpenExr:
                return "openexr";
            }
            return "";
        }

        // PNG colour type, bit depth and interlace method come from the 13-byte IHDR chunk (file offsets 16 to
        // 28), because OIIO expands palettes and low bit depths and keeps no attribute that says so.
        void readPngHeader(const char *path, SourceTraits &t) {
            FILE *f = std::fopen(path, "rb");
            if (f == nullptr)
                return;
            unsigned char b[33];
            size_t n = std::fread(b, 1, sizeof(b), f);
            std::fclose(f);
            static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
            if (n < sizeof(b) || std::memcmp(b, sig, 8) != 0 || std::memcmp(b + 12, "IHDR", 4) != 0)
                return;
            t.storedBits = b[24];
            t.pngColorType = b[25];
            t.pngInterlace = b[28];
        }

    } // namespace

    struct Reader::Impl {
            std::unique_ptr<OIIO::ImageInput> input;
            std::string path;
            OIIO::TypeDesc storedType;
            int channels = 0;
    };

    Reader::Reader() : impl(new Impl) {}

    Reader::~Reader() {
        close();
        delete impl;
    }

    bool Reader::open(const char *path, Format expected, SourceTraits &traits) {
        (void)oiioVersion(); // the one-time OIIO setup
        close();
        try {
            std::unique_ptr<OIIO::ImageInput> in = OIIO::ImageInput::create(formatName(expected));
            if (!in) {
                error(CODE_SYSTEM, "OpenImageIO has no reader for the %s format (\"%s\"): %s\n", formatName(expected), path,
                      OIIO::geterror().c_str());
                return false;
            }
            OIIO::ImageSpec config;
            config["oiio:UnassociatedAlpha"] = 1;
            config["oiio:RawColor"] = 1;
            OIIO::ImageSpec spec;
            if (!in->open(path, spec, config)) {
                std::string why = in->geterror();
                error(CODE_NOFILE, "Failed to open \"%s\": %s\n", path, why.c_str());
                return false;
            }
            if (std::strcmp(in->format_name(), formatName(expected)) != 0) {
                error(CODE_BADTOKEN, "\"%s\" is not a %s file (it reads as %s)\n", path, formatName(expected), in->format_name());
                in->close();
                return false;
            }

            SourceTraits t;
            t.format = expected;
            t.formatName = in->format_name();
            t.width = spec.width;
            t.height = spec.height;
            t.channels = spec.nchannels;
            t.storedType = spec.format.c_str();
            const OIIO::TypeDesc::BASETYPE base = (OIIO::TypeDesc::BASETYPE)spec.format.basetype;
            switch (base) {
            case OIIO::TypeDesc::HALF:
            case OIIO::TypeDesc::FLOAT:
            case OIIO::TypeDesc::DOUBLE:
                t.storedKind = StoredKind::Float;
                break;
            case OIIO::TypeDesc::INT8:
            case OIIO::TypeDesc::INT16:
            case OIIO::TypeDesc::INT32:
            case OIIO::TypeDesc::INT64:
                t.storedKind = StoredKind::Int;
                break;
            default:
                t.storedKind = StoredKind::UInt;
                break;
            }
            t.storedBits = spec.get_int_attribute("oiio:BitsPerSample", (int)spec.format.size() * 8);
            t.tiled = spec.tile_width > 0;
            t.planarSeparate = spec.get_int_attribute("tiff:PlanarConfiguration", 1) == 2;
            t.photometric = expected == Format::Tiff ? spec.get_int_attribute("tiff:PhotometricInterpretation", 0) : 0;
            t.deep = spec.deep;
            t.channelNames = spec.channelnames;
            // a second subimage (a multi-part EXR, a multi-page TIFF), counted only up to 2
            if (in->seek_subimage(1, 0)) {
                t.subimages = 2;
                in->seek_subimage(0, 0);
            }
            else {
                (void)in->geterror(); // the probe records "could not seek to subimage 1"; a clean slate for read()
            }
            if (expected == Format::Png)
                readPngHeader(path, t);

            impl->input = std::move(in);
            impl->path = path;
            impl->storedType = spec.format;
            impl->channels = spec.nchannels;
            traits = t;
            return true;
        } catch (const std::exception &e) {
            error(CODE_SYSTEM, "OpenImageIO failed on \"%s\": %s\n", path, e.what());
        } catch (...) {
            error(CODE_SYSTEM, "OpenImageIO failed on \"%s\"\n", path);
        }
        close();
        return false;
    }

    bool Reader::read(void *dest, SampleType delivered) {
        if (!impl->input) {
            return false;
        }
        const char *path = impl->path.c_str();
        try {
            OIIO::TypeDesc want;
            switch (delivered) {
            case SampleType::UInt8:
                want = OIIO::TypeDesc::UINT8;
                break;
            case SampleType::UInt16:
                want = OIIO::TypeDesc::UINT16;
                break;
            case SampleType::Float:
                want = OIIO::TypeDesc::FLOAT;
                break;
            default:
                error(CODE_BUG, "\"%s\": a half buffer cannot be delivered\n", path);
                return false;
            }
            // only half may be converted, to float; any other difference is a caller bug
            const bool same = impl->storedType == want;
            const bool halfToFloat = impl->storedType == OIIO::TypeHalf && delivered == SampleType::Float;
            if (!same && !halfToFloat) {
                error(CODE_BUG, "\"%s\": a %s source cannot be delivered as %s\n", path, impl->storedType.c_str(), want.c_str());
                return false;
            }
            const bool ok = impl->input->read_image(0, 0, 0, impl->channels, want, dest);
            // An OIIO reader may record an error and still return true (a truncated file); the pixels are then
            // not all there, so a recorded error is a failure too.
            if (!ok || impl->input->has_error()) {
                std::string why = impl->input->geterror();
                error(CODE_SYSTEM, "Failed to read the pixels of \"%s\": %s\n", path, why.empty() ? "unknown error" : why.c_str());
                return false;
            }
            return true;
        } catch (const std::exception &e) {
            error(CODE_SYSTEM, "OpenImageIO failed reading \"%s\": %s\n", path, e.what());
        } catch (...) {
            error(CODE_SYSTEM, "OpenImageIO failed reading \"%s\"\n", path);
        }
        return false;
    }

    bool Reader::matrix(const char *name, float m[16]) const {
        if (!impl->input)
            return false;
        try {
            const OIIO::ParamValue *p = impl->input->spec().find_attribute(name, OIIO::TypeMatrix);
            if (p == nullptr)
                return false;
            std::memcpy(m, p->data(), 16 * sizeof(float));
            return true;
        } catch (...) {
            return false;
        }
    }

    void Reader::close() {
        try {
            if (impl->input)
                impl->input->close();
        } catch (...) {
        }
        impl->input.reset();
    }

} // namespace orimageio
