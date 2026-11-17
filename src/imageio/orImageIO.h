// orImageIO.h -- the published interface of libRIBImageIO, the Image I/O utility library (spec 048,
// constitution 1.5.0, Principle VIII "Utility libraries").
//
// The library wraps OpenImageIO: pixel input and output only, no colour conversion. This header includes
// standard headers only and names no OpenImageIO, OpenColorIO, Imath or libtiff type, so a consumer needs no
// OIIO include path and instantiates no OIIO template. It is internal for now (not in OPENRENDER_PUBLIC_HEADERS):
// the interface stays private to the Texturing and Display Outputs domains until spec 049.
//
// Only orimageio:: symbols leave the shared library (exports.txt on macOS, hidden visibility on Linux).
#pragma once

#include <string>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define ORIMAGEIO_API __attribute__((visibility("default")))
#else
#define ORIMAGEIO_API
#endif

namespace orimageio {

    // The file formats the library reads and writes through OIIO.
    enum class Format { Tiff, Png, Rgbe, OpenExr };

    // The sample type of a buffer handed to or taken from the library. Half is a storage type for writing only.
    enum class SampleType { UInt8, UInt16, Half, Float };

    // How a source stores its samples (what the file says, before any conversion).
    enum class StoredKind { UInt, Int, Float };

    // The facts a Reader learns about a source after opening it. The library reports facts only; the caller
    // (Texturing's COiioImageInput) applies the acceptance policy to them (research R7, data-model section 3).
    struct SourceTraits {
            Format format = Format::Tiff;
            std::string formatName;           // the format OIIO's reader reports after open
            int width = 0, height = 0, channels = 0;
            int storedBits = 0;               // bits per sample as stored (TIFF and PNG report the file's own)
            StoredKind storedKind = StoredKind::UInt;
            std::string storedType;           // OIIO's name of the stored type: "uint8", "uint16", "half", "float", "uint32", "int16", ...
            bool tiled = false;               // a tiled source
            bool planarSeparate = false;      // TIFF PLANARCONFIG_SEPARATE
            int photometric = 0;              // TIFF photometric code, 0 if not TIFF
            int subimages = 1;                // the count, stopping at 2
            bool deep = false;
            std::vector<std::string> channelNames; // stored order
            int pngColorType = -1;            // PNG IHDR colour type (byte 25), -1 if not PNG
            int pngInterlace = -1;            // PNG IHDR interlace method (byte 28), -1 if not PNG
    };

    // One-shot reader of one source image: open, read once, close. A Reader is used by one thread at a time;
    // several may be open at once on different threads. Failures are reported through error() naming the file
    // and OIIO's message, and nothing throws out of the library.
    class ORIMAGEIO_API Reader {
        public:
            Reader();
            ~Reader();
            Reader(const Reader &) = delete;
            Reader &operator=(const Reader &) = delete;

            // Opens path as the expected format (never by probing: content and suffix must agree) with the
            // samples stored exactly as in the file (no alpha association, no colour conversion). False on failure.
            bool open(const char *path, Format expected, SourceTraits &traits);

            // Reads the whole image once into dest, tightly packed, row-major, channels in stored order.
            // `delivered` must be the stored type, except that Half is delivered as Float. False on failure.
            bool read(void *dest, SampleType delivered);

            // The 4x4 float matrix stored with the source ("worldtocamera" or "worldtoscreen"), valid after open.
            bool matrix(const char *name, float m[16]) const;

            void close(); // idempotent

        private:
            struct Impl;
            Impl *impl;
    };

    // The OpenImageIO version this library was built against, e.g. "3.1.14". One-time OIIO setup is done lazily
    // inside every entry point of the library (std::call_once), this one included.
    ORIMAGEIO_API const char *oiioVersion();

} // namespace orimageio
