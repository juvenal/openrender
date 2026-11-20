// scanlineWriter.cpp -- orimageio::ScanlineWriter, the file of a display written through OpenImageIO (spec 048, T109).
//
// The display drivers (src/display/oiio) hand it the rows the legacy drivers would have written: the samples are already quantized, dithered
// and in their final type, so no conversion of values happens here except the one the format asks for (float to half for OpenEXR). No colour
// conversion API is ever called.
#include "orImageIO.h"

#include "common/diagnostics.h"

#include <OpenImageIO/imageio.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

namespace orimageio {

    namespace {

        const char *oiioFormatName(Format f) {
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

        OIIO::TypeDesc storedType(SampleType t) {
            switch (t) {
            case SampleType::UInt8:
                return OIIO::TypeUInt8;
            case SampleType::UInt16:
                return OIIO::TypeUInt16;
            case SampleType::UInt32:
                return OIIO::TypeUInt32;
            case SampleType::Half:
                return OIIO::TypeHalf;
            case SampleType::Float:
                return OIIO::TypeFloat;
            }
            return OIIO::TypeUnknown;
        }

        // the type of the rows the driver delivers: the stored type, except Half, which arrives as float
        OIIO::TypeDesc deliveredType(SampleType t) { return t == SampleType::Half ? OIIO::TypeFloat : storedType(t); }

        // R, G, B, A, Z by position (the names the legacy OpenEXR driver gives), then Ch5, Ch6, ...
        std::string channelName(int i) {
            static const char *const names[] = {"R", "G", "B", "A", "Z"};
            return i < 5 ? names[i] : "Ch" + std::to_string(i);
        }

        bool equalsIgnoreCase(const char *a, const char *b) {
            for (; *a != '\0' && *b != '\0'; a++, b++)
                if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b))
                    return false;
            return *a == *b;
        }

        // the compression name OIIO wants; null (the format's default) for a name the legacy driver would not have recognised either
        const char *tiffCompression(const char *name) {
            if (name == nullptr)
                return "lzw"; // the legacy driver's default
            if (equalsIgnoreCase(name, "lzw"))
                return "lzw";
            if (equalsIgnoreCase(name, "jpeg") || equalsIgnoreCase(name, "jpg"))
                return "jpeg";
            if (equalsIgnoreCase(name, "deflate") || equalsIgnoreCase(name, "zip"))
                return "zip";
            if (equalsIgnoreCase(name, "none"))
                return "none";
            return "lzw";
        }

        // the legacy OpenEXR driver recognises only these upper case names and leaves the default (ZIP) for any other
        const char *exrCompression(const char *name) {
            if (name == nullptr)
                return "zip";
            if (std::strcmp(name, "RLE") == 0)
                return "rle";
            if (std::strcmp(name, "ZIPS") == 0)
                return "zips";
            if (std::strcmp(name, "ZIP") == 0)
                return "zip";
            if (std::strcmp(name, "PIZ") == 0)
                return "piz";
            if (std::strcmp(name, "PXR24") == 0)
                return "pxr24";
            return "zip";
        }

    } // namespace

    struct ScanlineWriter::Impl {
            std::string path;
            std::unique_ptr<OIIO::ImageOutput> out;
            OIIO::TypeDesc delivered = OIIO::TypeUnknown;
            bool started = false; // OIIO opened the file: it is ours to remove on failure
            bool failed = false;
            bool closed = false;
    };

    ScanlineWriter::ScanlineWriter(const char *path) : impl(new Impl) {
        (void)oiioVersion(); // the one-time OIIO setup
        impl->path = path;
    }

    ScanlineWriter::~ScanlineWriter() {
        close();
        delete impl;
    }

    bool ScanlineWriter::open(const ScanlineDesc &d) {
        try {
            impl->out = OIIO::ImageOutput::create(oiioFormatName(d.format));
            if (impl->out == nullptr) {
                error(CODE_SYSTEM, "OpenImageIO has no writer for the %s format (\"%s\"): %s\n", oiioFormatName(d.format), impl->path.c_str(), OIIO::geterror().c_str());
                impl->failed = true;
                return false;
            }

            OIIO::ImageSpec spec(d.width, d.height, d.channels, storedType(d.type));
            spec.channelnames.clear();
            for (int c = 0; c < d.channels; c++)
                spec.channelnames.push_back(channelName(c));

            switch (d.format) {
            case Format::Tiff:
                // associated alpha (the renderer's colour is premultiplied) is what the legacy driver declares for 4 samples; no value is converted
                spec.alpha_channel = d.channels == 4 ? 3 : -1;
                spec.attribute("compression", tiffCompression(d.compression));
                spec.attribute("Orientation", 1);
                spec.attribute("ResolutionUnit", "none");
                spec.attribute("XResolution", 1.0f);
                spec.attribute("YResolution", 1.0f);
                if (d.worldToScreen != nullptr)
                    spec.attribute("worldtoscreen", OIIO::TypeMatrix, d.worldToScreen);
                if (d.worldToCamera != nullptr)
                    spec.attribute("worldtocamera", OIIO::TypeMatrix, d.worldToCamera);
                break;
            case Format::Png:
                // PNG alpha is straight: the samples are written as given, with no unpremultiplication
                spec.alpha_channel = d.channels == 2 ? 1 : d.channels == 4 ? 3 : -1;
                spec["oiio:UnassociatedAlpha"] = 1;
                break;
            case Format::OpenExr:
                spec.attribute("compression", exrCompression(d.compression));
                break;
            case Format::Rgbe:
                break;
            }
            if (d.software != nullptr)
                spec.attribute("Software", d.software);

            if (!impl->out->open(impl->path, spec)) {
                error(CODE_SYSTEM, "Failed to create \"%s\" for writing: %s\n", impl->path.c_str(), impl->out->geterror().c_str());
                impl->failed = true;
                return false;
            }
            impl->started = true;
            impl->delivered = deliveredType(d.type);
            return true;
        }
        catch (const std::exception &e) {
            error(CODE_SYSTEM, "Failed to create \"%s\" for writing: %s\n", impl->path.c_str(), e.what());
        }
        catch (...) {
            error(CODE_SYSTEM, "Failed to create \"%s\" for writing: unknown exception\n", impl->path.c_str());
        }
        impl->failed = true;
        return false;
    }

    bool ScanlineWriter::writeScanline(int y, const void *pixels) {
        if (impl->failed || impl->closed || impl->out == nullptr)
            return false;
        try {
            if (!impl->out->write_scanline(y, 0, impl->delivered, pixels)) {
                error(CODE_SYSTEM, "Failed to write \"%s\": %s\n", impl->path.c_str(), impl->out->geterror().c_str());
                impl->failed = true;
                return false;
            }
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

    bool ScanlineWriter::close() {
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
