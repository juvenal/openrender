/**
 * Project: openRender
 *
 * File: texmake.cpp
 *
 * Description:
 *   This file implements the functionality for texmake.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	texmake.cpp
//  Classes				:	-
//  Description			:	This file comtains texture file making routines
//
////////////////////////////////////////////////////////////////////////
#include "texmake.h"
#include "common/algebra.h"
#include "common/diagnostics.h"
#include "imageInput.h"
#include "imageInputTiff.h"
#ifdef HAVE_OIIO
#include "orImageIO.h"
#endif
#include "common/memory.h"
#include "common/riVocabulary.h"
#include "ri_config.h"
#include "tiff.h"

#include <math.h>
#include <stddef.h> // ensure we have NULL defined before libtiff
#include <tiffio.h>

const char *TIFF_TEXTURE = "openRender Texture";
const char *TIFF_CYLINDER_ENVIRONMENT = "openRender Environment (cylinder)";
const char *TIFF_CUBIC_ENVIRONMENT = "openRender Environment (cubic)";
const char *TIFF_SPHERICAL_ENVIRONMENT = "openRender Environment (spherical)";
const char *TIFF_SHADOW = "openRender shadow";

const char *resizeUpMode = "up";
// resizeDownMode/resizeRoundMode/resizeNoneMode moved to texmake.h
// (spec 020-runtime-tile-paging, T006) -- used only inside adjustSize<T>,
// which moved there too for reachability from texture.cpp.

///////////////////////////////////////////////////////////////////////
// function				:	pixelSizeFromInfo
// Description			:	Bytes per pixel for a decoded CImageInfo, matching
//							the bit-depth tiers readLayer() used to compute inline
// Return Value			:	bytes per pixel
// Comments				:
static int pixelSizeFromInfo(const CImageInfo &info) {
    if (info.bitsPerSample == 8)
        return info.numChannels * sizeof(unsigned char);
    else if (info.bitsPerSample == 16)
        return info.numChannels * sizeof(unsigned short);
    else
        return info.numChannels * sizeof(float);
}

///////////////////////////////////////////////////////////////////////
// function				:	tiffErrorHandler
// Description			:	Handle errors coming from the libtiff
// Return Value			:	-
// Comments				:
static void tiffErrorHandler(const char *, const char *fmt, va_list ap) {
    char tmp[1024];

    vsnprintf(tmp, sizeof(tmp), fmt, ap);

    error(CODE_SYSTEM, "TIFF: %s\n", tmp);
}

///////////////////////////////////////////////////////////////////////
// Function				:	legacyAppendLayer
// Description			:	Append a layer of image into an image file (the libtiff writer; CLegacyBakeSink::appendLayer
//							calls it, spec 048)
// Return Value			:	-
// Comments				:
static void legacyAppendLayer(TIFF *out, int numSamples, int bitsperpixel, int tileSize, int width, int height, void *data) {
    int x, y;
    unsigned char *tileData;
    int pixelSize;

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, (unsigned long)width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, (unsigned long)height);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);
    TIFFSetField(out, TIFFTAG_XRESOLUTION, 1.0f);
    TIFFSetField(out, TIFFTAG_YRESOLUTION, 1.0f);
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_ADOBE_DEFLATE);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_JPEG);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_PACKBITS);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_THUNDERSCAN);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_PIXARFILM);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_PIXARLOG);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_DEFLATE);

    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, (unsigned long)numSamples);
    TIFFSetField(out, TIFFTAG_TILEWIDTH, (unsigned long)tileSize);
    TIFFSetField(out, TIFFTAG_TILELENGTH, (unsigned long)tileSize);

    // PHOTOMETRIC (and, for a trailing alpha channel, EXTRASAMPLES) tells
    // libtiff which samples are "color" vs "extra"; without it, libtiff
    // warns heavily on read for any numSamples != 1 (GitHub issue #18).
    // Scoped to the 8-bit and float branches only (bitsperpixel != 16) --
    // NOT the 16-bit branch, deliberately: readMadeTexture<unsigned short>()
    // (src/ri/texture/texture.cpp) has a pre-existing heuristic that
    // reads PHOTOMETRIC_RGB on a 16-bit texture as "this is a
    // pixar-txmake-style texture using a half-range (32k, not 65k) 16-bit
    // encoding" and applies a 2x value scale on load. That heuristic
    // predates this fix and this codebase's own 16-bit bakes do NOT use
    // that half-range encoding (appendLayer() below writes the full
    // 0-65535 SAMPLEFORMAT_UINT range) -- so naively setting
    // PHOTOMETRIC_RGB here for a 3/4-channel 16-bit bake would silently
    // double pixel values read back from freshly-baked textures. Fixing
    // that requires auditing/changing the read-side heuristic too, a
    // separate, higher-risk change; tracked as a follow-up on issue #18.
    // 16-bit multi-channel bakes still trigger the read-time warning until
    // that follow-up lands, but produce byte-identical pixel *values*.
    if (bitsperpixel != 16) {
        switch (numSamples) {
        case 1:
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            break;
        case 2: {
            // Grayscale + alpha.
            uint16_t extra[1] = {EXTRASAMPLE_UNASSALPHA};
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            TIFFSetField(out, TIFFTAG_EXTRASAMPLES, 1, extra);
            break;
        }
        case 3:
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            break;
        case 4: {
            // RGB + alpha. openRender does not premultiply alpha when
            // baking, so the alpha channel is unassociated (straight),
            // not associated.
            uint16_t extra[1] = {EXTRASAMPLE_UNASSALPHA};
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            TIFFSetField(out, TIFFTAG_EXTRASAMPLES, 1, extra);
            break;
        }
        default:
            // Unexpected channel count for anything this codebase
            // currently bakes -- leave PHOTOMETRIC unset rather than guess.
            break;
        }
    }

    if (bitsperpixel == 8) {
        TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, (unsigned long)(sizeof(unsigned char) * 8));
        pixelSize = numSamples * sizeof(unsigned char);
    }
    else if (bitsperpixel == 16) {
        TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, (unsigned long)(sizeof(unsigned short) * 8));
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK); // see comment above -- unchanged deliberately
        pixelSize = numSamples * sizeof(unsigned short);
    }
    else {
        TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, (unsigned long)(sizeof(float) * 8));
        pixelSize = numSamples * sizeof(float);
    }

    arenaBegin(globalArena);

    tileData = (unsigned char *)ralloc(pixelSize * tileSize * tileSize, globalArena);

    assert(TIFFTileSize(out) == (tileSize * tileSize * pixelSize));

    for (y = 0; y < height; y += tileSize) {
        for (x = 0; x < width; x += tileSize) {
            int ty;

            for (ty = 0; ty < tileSize; ty++) {
                memcpy(&tileData[ty * tileSize * pixelSize], &((unsigned char *)data)[((y + ty) * width + x) * pixelSize], tileSize * pixelSize);
            }

            TIFFWriteTile(out, tileData, x, y, 0, 0);
        }
    }

    TIFFWriteDirectory(out);

    arenaEnd(globalArena);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBakeSink
// Description			:	Where the layers of a baked texture go (spec 048). The make* functions compute the pyramid
//							(adjustSize, filterScaleImage, appendPyramid) and hand each layer to a sink: the legacy libtiff
//							writer, or the OpenImageIO writer.
// Comments				:	The tags set before appendLayer() apply to that directory only and are then cleared, which is
//							libtiff's TIFFWriteDirectory behaviour that the legacy writer relies on (the texture format and
//							the wrap modes on directory 0, the full size on the first directory of each pyramid).
class CBakeSink {
    public:
        virtual ~CBakeSink() {}

        virtual void setTextureFormat(const char *format) = 0;
        virtual void setWrapModes(const char *modes) = 0;
        virtual void setMatrices(const float *worldToCamera, const float *worldToScreen) = 0;
        virtual void setFullSize(int width, int height) = 0;
        virtual void appendLayer(int numSamples, int bitsperpixel, int tileSize, int width, int height, void *data) = 0;
        virtual bool close() = 0;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CLegacyBakeSink
// Description			:	The libtiff writer: the code appendLayer() and the make* functions always had, moved behind the sink
// Comments				:	Pinned byte for byte by ImageInput_TiffBakeByteIdentical and by legacy_bake_hashes --check.
class CLegacyBakeSink : public CBakeSink {
    public:
        explicit CLegacyBakeSink(TIFF *handle) : out(handle) {}
        ~CLegacyBakeSink() { close(); }

        void setTextureFormat(const char *format) { TIFFSetField(out, TIFFTAG_PIXAR_TEXTUREFORMAT, format); }
        void setWrapModes(const char *modes) { TIFFSetField(out, TIFFTAG_PIXAR_WRAPMODES, modes); }
        void setMatrices(const float *worldToCamera, const float *worldToScreen) {
            TIFFSetField(out, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, worldToCamera);
            TIFFSetField(out, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, worldToScreen);
        }
        void setFullSize(int width, int height) {
            TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLWIDTH, width);
            TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLLENGTH, height);
        }
        void appendLayer(int numSamples, int bitsperpixel, int tileSize, int width, int height, void *data) { legacyAppendLayer(out, numSamples, bitsperpixel, tileSize, width, height, data); }
        bool close() {
            if (out != NULL) {
                TIFFClose(out);
                out = NULL;
            }
            return true;
        }

    private:
        TIFF *out;
};

#ifdef HAVE_OIIO
///////////////////////////////////////////////////////////////////////
// Class				:	COiioBakeSink
// Description			:	The OpenImageIO writer (spec 048): the same pyramid, stored by orimageio::TiledTiffWriter
// Comments				:	Only (bits, channels) combinations that pass kOiioBakeExclusions get here. The tags set before
//							appendLayer() belong to that directory only, as in the legacy sink.
class COiioBakeSink : public CBakeSink {
    public:
        explicit COiioBakeSink(const char *path) : writer(path) {}

        void setTextureFormat(const char *format) { textureFormat = format; hasTextureFormat = true; }
        void setWrapModes(const char *modes) { wrapModes = modes; hasWrapModes = true; }
        void setMatrices(const float *worldToCamera, const float *worldToScreen) {
            memcpy(toCamera, worldToCamera, sizeof(toCamera));
            memcpy(toScreen, worldToScreen, sizeof(toScreen));
            hasMatrices = true;
        }
        void setFullSize(int width, int height) {
            fullWidth = width;
            fullHeight = height;
        }
        void appendLayer(int numSamples, int bitsperpixel, int tileSize, int width, int height, void *data) {
            orimageio::DirectoryDesc d;

            d.width = width;
            d.height = height;
            d.channels = numSamples;
            d.type = (bitsperpixel == 8) ? orimageio::SampleType::UInt8 : (bitsperpixel == 16) ? orimageio::SampleType::UInt16 : orimageio::SampleType::Float;
            d.tileSize = tileSize;
            d.fullWidth = fullWidth;
            d.fullHeight = fullHeight;
            if (hasTextureFormat)
                d.textureFormat = textureFormat.c_str();
            if (hasWrapModes)
                d.wrapModes = wrapModes.c_str();
            if (hasMatrices) {
                d.worldToCamera = toCamera;
                d.worldToScreen = toScreen;
            }

            writer.appendDirectory(d, data); // a failure is reported by the writer and surfaces in close()

            // the tags belong to the directory just written
            hasTextureFormat = hasWrapModes = hasMatrices = false;
            fullWidth = fullHeight = 0;
        }
        bool close() { return writer.close(); }

    private:
        orimageio::TiledTiffWriter writer;
        std::string textureFormat, wrapModes;
        float toCamera[16], toScreen[16];
        bool hasTextureFormat = false, hasWrapModes = false, hasMatrices = false;
        int fullWidth = 0, fullHeight = 0;
};
#endif

///////////////////////////////////////////////////////////////////////
// Struct				:	SOiioBakeExclusion
// Description			:	A (precision, channels) combination OpenImageIO cannot write the way the texture reader
//							expects (spec 048, data-model section 4)
// Comments				:	OIIO writes PHOTOMETRIC_RGB from 3 channels and EXTRASAMPLES from the channel count; the
//							legacy writer sets neither for a 16-bit directory (the reader scales a 16-bit RGB one, GitHub
//							#18) and leaves PHOTOMETRIC unset from 5 channels. Such a bake keeps the legacy writer.
struct SOiioBakeExclusion {
        int bits;        // 16, or 0 for every other precision (8-bit and float)
        int minChannels; // excluded from this channel count up
        const char *message;
};

static const SOiioBakeExclusion kOiioBakeExclusions[] = {
    {16, 3, "%s: 16-bit %d-channel textures are baked by the legacy writer (OpenImageIO writes PHOTOMETRIC_RGB, which the texture reader scales; GitHub #18)\n"},
    {0, 5, "%s: %d-channel textures are baked by the legacy writer (OpenImageIO writes PHOTOMETRIC_RGB; legacy leaves it unset)\n"},
};

///////////////////////////////////////////////////////////////////////
// Function				:	oiioBakeExclusion
// Description			:	The exclusion a (precision, channels) combination falls under
// Return Value			:	The table row, or NULL when OpenImageIO writes it
// Comments				:
static const SOiioBakeExclusion *oiioBakeExclusion(int bitsperpixel, int numSamples) {
    for (const SOiioBakeExclusion &e : kOiioBakeExclusions) {
        const bool precisionMatches = (e.bits == 16) ? (bitsperpixel == 16) : (bitsperpixel != 16);

        if (precisionMatches && numSamples >= e.minChannels)
            return &e;
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////
// Function				:	gateBakeBackend
// Description			:	Chooses the writer of a make* call: the selected backend, except for a combination in the
//							exclusion table, which the legacy writer bakes (one message, before the output exists)
// Return Value			:	The backend whose writer to use
// Comments				:	Called once per make* call. CODE_SYSTEM, not CODE_LOG: the RI error handler never sees CODE_LOG
//							(it goes to the log file only). A numSamples of 0 means "nothing to check" (a cubic bake with no face).
static EImageBackend gateBakeBackend(EImageBackend backend, const char *output, int bitsperpixel, int numSamples) {
    if (backend != EImageBackend::OIIO || numSamples == 0)
        return backend;

    const SOiioBakeExclusion *e = oiioBakeExclusion(bitsperpixel, numSamples);

    if (e == NULL)
        return backend;

    info(CODE_SYSTEM, e->message, (output != NULL) ? output : "", numSamples);

    return EImageBackend::Legacy;
}

///////////////////////////////////////////////////////////////////////
// Function				:	openBakeSink
// Description			:	Opens the writer of a make* call's output
// Return Value			:	The sink, or NULL when the output cannot be created
// Comments				:	backend is the gated one (gateBakeBackend); the OpenImageIO writer creates the file with its
//							first directory.
static CBakeSink *openBakeSink(const char *output, EImageBackend backend) {
    if (output == NULL)
        return NULL;

#ifdef HAVE_OIIO
    if (backend == EImageBackend::OIIO)
        return new COiioBakeSink(output);
#else
    (void)backend;
#endif

    TIFF *handle = TIFFOpen(output, "w");

    if (handle == NULL)
        return NULL;

    return new CLegacyBakeSink(handle);
}

///////////////////////////////////////////////////////////////////////
// Function				:	appendPyramid
// Description			:	Append an image pyramid into an image file
//							the reduction gives the amount of reduction in the image size at each step
// Return Value			:
// Comments				:
template <class T>
static void appendPyramid(CBakeSink &sink, int numSamples, int bitsperpixel, int tileSize, int width, int height, T *data) {
    T *currentLevel;
    int currentWidth, currentHeight, nextWidth, nextHeight;
    float *fnextLevel;

    // Append the base layer
    sink.appendLayer(numSamples, bitsperpixel, tileSize, width, height, data);

    // Append the remaining layers
    currentWidth = width;
    currentHeight = height;
    currentLevel = data;

    fnextLevel = (float *)ralloc(width * height * numSamples * sizeof(float), globalArena);

    int numLevels = tiffNumLevels(width, height);
    int i;

    for (i = 1; i < numLevels; i++) {
        int x, y, yo;
        int s;

        nextWidth = currentWidth >> 1;
        nextHeight = currentHeight >> 1;

        for (y = 0, yo = 0; y < nextHeight; y++, yo += 2) {
            T *src = &currentLevel[yo * currentWidth * numSamples];
            float *dest = &fnextLevel[y * nextWidth * numSamples];
            int n;

            for (x = 0; x < nextWidth; x++) {
                for (n = 0; n < numSamples; n++) {
                    dest[n] = src[n];
                    dest[n] += src[numSamples + n];
                }
                dest += numSamples;
                src += 2 * numSamples;
            }

            src = &currentLevel[(yo + 1) * currentWidth * numSamples];
            dest = &fnextLevel[y * nextWidth * numSamples];

            for (x = 0; x < nextWidth; x++) {
                for (n = 0; n < numSamples; n++) {
                    dest[n] += src[n];
                    dest[n] += src[numSamples + n];
                }
                dest += numSamples;
                src += 2 * numSamples;
            }

            dest = &fnextLevel[y * nextWidth * numSamples];

            for (x = 0; x < nextWidth * numSamples; x++) {
                *dest++ *= 1 / (float)4;
            }
        }

        for (s = 0; s < nextWidth * nextHeight * numSamples; s++) {
            currentLevel[s] = (T)fnextLevel[s];
        }

        currentWidth = nextWidth;
        currentHeight = nextHeight;

        sink.appendLayer(numSamples, bitsperpixel, tileSize, currentWidth, currentHeight, currentLevel);
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	readLayer
// Description			:	Read a layer of the image into memory
// Return Value			:
// Comments				:
void *readLayer(TIFF *in, int *width, int *height, int *bitsperpixel, int *numSamples) {
    unsigned char *data;
    int i;
    uint32_t w, h;
    uint16_t ns, bp;
    int pixelSize;

    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &ns);
    TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bp);

    width[0] = w;
    height[0] = h;
    numSamples[0] = ns;
    bitsperpixel[0] = bp;

    if (bp == 8) {
        pixelSize = ns * sizeof(unsigned char);
    }
    else if (bp == 16) {
        pixelSize = ns * sizeof(unsigned short);
    }
    else if (bp == 32) {
        pixelSize = ns * sizeof(float);
    }
    else {
        error(CODE_BUG, "Unknown bits per pixel in readLayer (%d)\n", bp);
        pixelSize = 0;
    }

    data = (unsigned char *)ralloc(pixelSize * w * h, globalArena);

    for (i = 0; i < (int)h; i++) {
        TIFFReadScanline(in, &data[i * pixelSize * w], i, 0);
    }

    return data;
}

// copyData<T>/initData<T>/initDataValues<T>/filterImage<T>/
// filterScaleImage<T>/adjustSize<T> moved to texmake.h (spec
// 020-runtime-tile-paging, T006) for reachability from texture.cpp --
// pure relocation, no behavior change; appendTexture() below is
// unaffected, now reached via the header instead of a local definition.

///////////////////////////////////////////////////////////////////////
// Function				:	appendTexture
// Description			:	Make and append a texture to the end of the TIFF file
// Return Value			:
// Comments				:	FIXME: filter only when adjusting size
static void appendTexture(CBakeSink &sink, int width, int height, int numSamples, int bitspersample, RtFilterFunc filter, float filterWidth, float filterHeight, int tileSize, void *data, const char *smode, const char *tmode, const char *resizemode) {
    int validHeight, validWidth;

    if (bitspersample == 8) {
        adjustSize<unsigned char>((unsigned char **)&data, &width, &height, &validWidth, &validHeight, numSamples, bitspersample, filterWidth, filterHeight, filter, smode, tmode, resizemode);

        // write adjusted sizes
        sink.setFullSize(validWidth, validHeight);

        appendPyramid<unsigned char>(sink, numSamples, bitspersample, tileSize, width, height, (unsigned char *)data);
    }
    else if (bitspersample == 16) {
        adjustSize<unsigned short>((unsigned short **)&data, &width, &height, &validWidth, &validHeight, numSamples, bitspersample, filterWidth, filterHeight, filter, smode, tmode, resizemode);

        // write adjusted sizes
        sink.setFullSize(validWidth, validHeight);

        appendPyramid<unsigned short>(sink, numSamples, bitspersample, tileSize, width, height, (unsigned short *)data);
    }
    else if (bitspersample == 32) {
        adjustSize<float>((float **)&data, &width, &height, &validWidth, &validHeight, numSamples, bitspersample, filterWidth, filterHeight, filter, smode, tmode, resizemode);

        // write adjusted sizes
        sink.setFullSize(validWidth, validHeight);

        appendPyramid<float>(sink, numSamples, bitspersample, tileSize, width, height, (float *)data);
    }
}

#define getResizeMode(numParams, params, vals)  \
    const char *resizeMode = resizeUpMode;      \
    for (int p = 0; p < numParams; p++) {       \
        if (strcmp(params[p], "resize") == 0) { \
            resizeMode = *((char **)vals[p]);   \
            break;                              \
        }                                       \
    }

///////////////////////////////////////////////////////////////////////
// Function				:	selectImageBackend
// Description			:	Reads the "imageio" parameter of a Make* call (spec 048): "legacy" or "oiio", given as
//							"imageio" or as an inline declaration ("string imageio"). With none, the call decodes
//							through the legacy decoders: the Option "imageio" "backend" reaches here as an "imageio"
//							parameter the renderer core appended (rendererContext.cpp), because Texturing has no
//							access to the options.
// Return Value			:	FALSE, with the error reported, when the value is unknown, or "oiio" in a build without
//							OpenImageIO: the call then fails and writes nothing
// Comments				:
static bool selectImageBackend(int numParams, const char **params, const void **vals, EImageBackend &backend) {
    backend = EImageBackend::Legacy;

    for (int p = 0; p < numParams; p++) {
        const char *space = strrchr(params[p], ' ');
        if (strcmp(space != NULL ? space + 1 : params[p], "imageio") != 0)
            continue;

        const char *value = *((const char **)vals[p]);
        if (strcmp(value, "legacy") == 0) {
            backend = EImageBackend::Legacy;
        }
        else if (strcmp(value, "oiio") == 0) {
            if (!imageBackendAvailable(EImageBackend::OIIO)) {
                error(CODE_BADTOKEN, OR_OIIO_NOT_BUILT_TEXT ": the \"imageio\" parameter asks for \"oiio\"\n");
                return false;
            }
            backend = EImageBackend::OIIO;
        }
        else {
            error(CODE_BADTOKEN, "Unknown image backend \"%s\" (expected \"legacy\" or \"oiio\")\n", value);
            return false;
        }
        break;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeTexture
// Description			:	Create an image pyramid from input
// Return Value			:
// Comments				:
void makeTexture(const char *input, const char *output, const CTextureFileResolver *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    EImageBackend imageBackend;
    if (!selectImageBackend(numParams, params, vals, imageBackend))
        return;

    if (textureLocate(path, inputFileName, input) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName, imageBackend);
        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
                void *data;
                int numSamples = imageInfo.numChannels;
                int bitspersample = imageInfo.bitsPerSample;
                int width = imageInfo.width, height = imageInfo.height;
                int tileSize = DEFAULT_TILE_SIZE;
                RtFilterFunc filter = filt;
                float filterWidth = fwidth;
                float filterHeight = fheight;
                char modes[128];

                arenaBegin(globalArena);

                // Read the texture
                data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, globalArena);
                imageInput->readImage(data);
                imageInput->close();
                delete imageInput;

                // Write the made texture
                CBakeSink *sink = openBakeSink(output, gateBakeBackend(imageBackend, output, bitspersample, numSamples));
                if (sink != NULL) {
                    snprintf(modes, sizeof(modes), "%s,%s", smode, tmode);

                    sink->setTextureFormat(TIFF_TEXTURE);
                    sink->setWrapModes(modes);

                    appendTexture(*sink, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);

                    sink->close();
                    delete sink;
                }

                arenaEnd(globalArena);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeSideEnvironment
// Description			:	Create a single sided environment map
// Return Value			:
// Comments				:
void makeSideEnvironment(const char *input, const char *output, const CTextureFileResolver *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    EImageBackend imageBackend;
    if (!selectImageBackend(numParams, params, vals, imageBackend))
        return;

    if (textureLocate(path, inputFileName, input) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName, imageBackend);
        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
                void *data;
                int numSamples = imageInfo.numChannels;
                int bitspersample = imageInfo.bitsPerSample;
                int width = imageInfo.width, height = imageInfo.height;
                int tileSize = DEFAULT_TILE_SIZE;
                RtFilterFunc filter = filt;
                float filterWidth = fwidth;
                float filterHeight = fheight;
                matrix worldToCamera, worldToScreen;

                arenaBegin(globalArena);

                // Read off the from world transformation from the image if possible.
                // These are Pixar-private TIFF tags with no equivalent in any other
                // source format, so a non-TIFF source always takes the same
                // "tag missing" fallback (identity matrix + error) a TIFF source
                // without these tags already took before this feature existed.
                if (!imageInput->readMatrix("worldtocamera", worldToCamera)) {
                    error(CODE_BUG, "Failed to read the world to camera matrix\n");
                    identitym(worldToCamera);
                }

                if (!imageInput->readMatrix("worldtoscreen", worldToScreen)) {
                    error(CODE_BUG, "Failed to read the world to screen matrix\n");
                    identitym(worldToScreen);
                }

                // Read the data
                data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, globalArena);
                imageInput->readImage(data);

                // Close the input
                imageInput->close();
                delete imageInput;

                CBakeSink *sink = openBakeSink(output, gateBakeBackend(imageBackend, output, bitspersample, numSamples));
                if (sink != NULL) {
                    // Write the texture data
                    sink->setTextureFormat(TIFF_SHADOW);
                    sink->setMatrices(worldToCamera, worldToScreen);

                    appendTexture(*sink, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);

                    sink->close();
                    delete sink;
                }
                else {
                    error(CODE_SYSTEM, "Failed to create \"%s\" for writing\n", output);
                }

                arenaEnd(globalArena);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeCubicEnvironment
// Description			:	Create an environment map where each side is a pyramid
// Return Value			:
// Comments				:
void makeCubicEnvironment(const char *px, const char *py, const char *pz, const char *nx, const char *ny, const char *nz, const char *output, const char *smode, const char *tmode, const CTextureFileResolver *path, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int) {
    char inputFileName[OS_MAX_PATH_LENGTH];
    const char *names[6];

    getResizeMode(numParams, params, vals);

    EImageBackend imageBackend;
    if (!selectImageBackend(numParams, params, vals, imageBackend))
        return;

    names[0] = px;
    names[1] = nx;
    names[2] = py;
    names[3] = ny;
    names[4] = pz;
    names[5] = nz;

    if (textureLocate(path, inputFileName, names[0]) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", names[0]);
    }
    else {

        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        // The writer is chosen once for the six faces: OpenImageIO unless a face is in the exclusion table. The faces'
        // headers are read here only for that, and only when OpenImageIO is selected.
        int gateBits = 0, gateSamples = 0;
        int unreadableFace = -1; // a face whose header open failed here: already reported, the loop below stops at it
        if (imageBackend == EImageBackend::OIIO) {
            for (int i = 0; i < 6; i++) {
                CImageInfo faceInfo;
                CImageInput *faceInput;

                if (textureLocate(path, inputFileName, names[i]) == FALSE)
                    break;
                faceInput = createImageInput(inputFileName, imageBackend);
                if (faceInput == NULL)
                    break;
                bool opened = faceInput->open(inputFileName, faceInfo);
                if (opened)
                    faceInput->close();
                delete faceInput;
                if (!opened) {
                    unreadableFace = i;
                    break;
                }
                if (i == 0 || oiioBakeExclusion(faceInfo.bitsPerSample, faceInfo.numChannels) != NULL) {
                    gateBits = faceInfo.bitsPerSample;
                    gateSamples = faceInfo.numChannels;
                    if (oiioBakeExclusion(gateBits, gateSamples) != NULL)
                        break;
                }
            }
        }

        CBakeSink *sink = openBakeSink(output, gateBakeBackend(imageBackend, output, gateBits, gateSamples));
        if (output != NULL) {
            void *data;
            int numSamples;
            int bitspersample;
            int i;
            int tileSize = DEFAULT_TILE_SIZE;
            RtFilterFunc filter = filt;
            float filterWidth = fwidth;
            float filterHeight = fheight;

            if (sink != NULL) {
                sink->setTextureFormat(TIFF_CUBIC_ENVIRONMENT);

                for (i = 0; i < 6; i++) {
                    int width, height;
                    CImageInput *faceInput;

                    // Open the file
                    if (i == unreadableFace) {
                        break; // its open failed above and was reported there
                    }
                    else if (textureLocate(path, inputFileName, names[i]) == FALSE) {
                        error(CODE_NOFILE, "Failed to find \"%s\"\n", names[i]);
                        break;
                    }
                    else {
                        faceInput = createImageInput(inputFileName, imageBackend);
                        if (faceInput == NULL)
                            break;
                        CImageInfo faceInfo;
                        if (!faceInput->open(inputFileName, faceInfo)) {
                            delete faceInput;
                            break;
                        }

                        arenaBegin(globalArena);

                        width = faceInfo.width;
                        height = faceInfo.height;
                        numSamples = faceInfo.numChannels;
                        bitspersample = faceInfo.bitsPerSample;

                        // Read the data
                        data = ralloc(pixelSizeFromInfo(faceInfo) * width * height, globalArena);
                        faceInput->readImage(data);
                        faceInput->close();
                        delete faceInput;

                        // Write the data
                        appendTexture(*sink, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);

                        arenaEnd(globalArena);
                    }
                }

                sink->close();
                delete sink;
            }
        }
        else {
            error(CODE_SYSTEM, "Failed to create \"%s\" for writing\n", output);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeSphericalEnvironment
// Description			:	Create an image pyramid from input
// Return Value			:
// Comments				:
void makeSphericalEnvironment(const char *input, const char *output, const CTextureFileResolver *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    EImageBackend imageBackend;
    if (!selectImageBackend(numParams, params, vals, imageBackend))
        return;

    if (textureLocate(path, inputFileName, input) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName, imageBackend);

        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
                void *data;
                int numSamples = imageInfo.numChannels;
                int bitspersample = imageInfo.bitsPerSample;
                int width = imageInfo.width, height = imageInfo.height;
                int tileSize = DEFAULT_TILE_SIZE;
                RtFilterFunc filter = filt;
                float filterWidth = fwidth;
                float filterHeight = fheight;
                char modes[128];

                arenaBegin(globalArena);

                // Read the texture
                data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, globalArena);
                imageInput->readImage(data);
                imageInput->close();
                delete imageInput;

                CBakeSink *sink = openBakeSink(output, gateBakeBackend(imageBackend, output, bitspersample, numSamples));
                if (sink != NULL) {
                    snprintf(modes, sizeof(modes), "%s,%s", smode, tmode);

                    // Write out the data
                    sink->setTextureFormat(TIFF_SPHERICAL_ENVIRONMENT);
                    sink->setWrapModes(modes);

                    appendTexture(*sink, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);
                    sink->close();
                    delete sink;
                }

                arenaEnd(globalArena);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeSphericalEnvironment
// Description			:	Create an image pyramid from input
// Return Value			:
// Comments				:
void makeCylindericalEnvironment(const char *input, const char *output, const CTextureFileResolver *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    EImageBackend imageBackend;
    if (!selectImageBackend(numParams, params, vals, imageBackend))
        return;

    if (textureLocate(path, inputFileName, input) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName, imageBackend);

        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
            void *data;
            int numSamples = imageInfo.numChannels;
            int bitspersample = imageInfo.bitsPerSample;
            int width = imageInfo.width, height = imageInfo.height;
            int tileSize = DEFAULT_TILE_SIZE;
            RtFilterFunc filter = filt;
            float filterWidth = fwidth;
            float filterHeight = fheight;
            char modes[128];

            arenaBegin(globalArena);

            data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, globalArena);
            imageInput->readImage(data);
            imageInput->close();
            delete imageInput;

            CBakeSink *sink = openBakeSink(output, gateBakeBackend(imageBackend, output, bitspersample, numSamples));
            if (sink != NULL) {
                snprintf(modes, sizeof(modes), "%s,%s", smode, tmode);

                sink->setTextureFormat(TIFF_CYLINDER_ENVIRONMENT);
                sink->setWrapModes(modes);

                appendTexture(*sink, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);
                sink->close();
                delete sink;
            }

            arenaEnd(globalArena);
            }
        }
    }
}
