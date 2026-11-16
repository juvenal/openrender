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

    // The OpenImageIO version this library was built against, e.g. "3.1.14". One-time OIIO setup is done lazily
    // inside every entry point of the library (std::call_once), this one included.
    ORIMAGEIO_API const char *oiioVersion();

} // namespace orimageio
