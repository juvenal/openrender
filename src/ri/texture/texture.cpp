/**
 * Project: openRender
 *
 * File: texture.cpp
 *
 * Description:
 *   This file implements the functionality for texture.
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
//  File				:	texture.cpp
//  Classes				:	CTexture
//  Description			:	Implementation !!!
//
////////////////////////////////////////////////////////////////////////
#include "texture.h"
#include "common/portable_io.h"
#include "error.h"
#include "imageInput.h"
#include "memory.h"
#include "renderer.h"
#include "ri_config.h"
#include "shading.h"
#include "stats.h"
#include "texmake.h"
#include "tiff.h"
#include "tileSource.h"

#include <algorithm>
#include <cstdio> // rename()
#include <functional> // std::hash
#include <math.h>
#include <memory>
#include <stddef.h> // Ensure NULL is defined before libtiff
#include <string.h>
#include <string>
#include <sys/stat.h> // stat(), for source-mtime and cache-key computation
#include <tiffio.h>
#include <unistd.h> // getpid()
#include <vector>

///////////////////////////////////////////////////////////////////////
// Class				:	CTexBlockThreadData
// Description			:	This class holds thread-specific information about a texture block
// Comments				:
class CTexBlockThreadData {
    public:
        void *data;        // Residency for each thread
        int lastRefNumber; // Last time this block was referenced
};
// FIXME.
// NOT CURRENTLY USED.
// INTENDED AS TAIL-PATCH to CTextureBlock

///////////////////////////////////////////////////////////////////////
// Class				:	CTextureBlock
// Description			:	This class holds information about a particular texture block
// Comments				:
class CTextureBlock {
    public:
        void *data; // Where the block data is stored (NULL if the block has been paged out)
        CTexBlockThreadData *threadData;

#ifdef TEXTURE_PERBLOCK_LOCK
        TMutex mutex;
#endif

        int refCount;        // how many threads reference this block
        int size;            // Size of the block in bytes
        CTextureBlock *next; // Pointer to the next used / empty block
        CTextureBlock *prev; // Pointer to the previous used / empty block
};

///////////////////////////////////////////////////////////////////////
//
//		Misc texture block management functions
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// function				:	tiffErrorHandler
// Description			:	Handle errors coming from the libtiff
// Return Value			:	-
// Comments				:
static void tiffErrorHandler(const char *, const char *fmt, va_list ap) {
    char tmp[1024];

    vsnprintf(tmp, sizeof(tmp), fmt, ap);

    error(CODE_SYSTEM, tmp);
}

///////////////////////////////////////////////////////////////////////
// Function				:	textureQuickSort
// Description			:	Sort the textures in the order of increasing last ref number
// Return Value			:
// Comments				:
static void textureQuickSort(CTextureBlock **activeBlocks, int start, int end, int thread) {
    int i, last;
    CTextureBlock *cBlock;

    for (last = start, i = start + 1; i <= end; i++) {
        if (activeBlocks[i]->threadData[thread].lastRefNumber < activeBlocks[start]->threadData[thread].lastRefNumber) {
            last++;
            cBlock = activeBlocks[last];
            activeBlocks[last] = activeBlocks[i];
            activeBlocks[i] = cBlock;
        }
    }

    cBlock = activeBlocks[last];
    activeBlocks[last] = activeBlocks[start];
    activeBlocks[start] = cBlock;

    // Speed is not an issue since this is not done very frequently, so recursion is OK
    if ((last - 1) > start)
        textureQuickSort(activeBlocks, start, last - 1, thread);

    if (end > (last + 1))
        textureQuickSort(activeBlocks, last + 1, end, thread);
}

///////////////////////////////////////////////////////////////////////
// Function				:	textureMemFlush
// Description			:	Try to deallocate some textures from memory
// Return Value			:
// Comments				:
static inline void textureMemFlush(CTextureBlock *entry, CShadingContext *context, int all) {

    // Do we have stuff to free ?
    if (CRenderer::textureUsedBlocks == NULL)
        return;

#ifdef TEXTURE_PERBLOCK_LOCK
    osLock(CRenderer::textureMutex);
#endif

    memBegin(context->threadMemory);

    const int thread = context->thread;

    // Figure out how many blocks we have in memory
    int i, j;
    CTextureBlock **activeBlocks;
    CTextureBlock *cBlock;
    for (cBlock = CRenderer::textureUsedBlocks, i = 0; cBlock != NULL; cBlock = cBlock->next) {
        if (cBlock->threadData[thread].data != NULL) {
            i++;
        }
    }

    // Collect those blocks into an array
    activeBlocks = (CTextureBlock **)ralloc(i * sizeof(CTextureBlock *), context->threadMemory);
    for (cBlock = CRenderer::textureUsedBlocks, i = 0; cBlock != NULL; cBlock = cBlock->next) {
        if (cBlock->threadData[thread].data != NULL) {
            if (cBlock != entry) {
                activeBlocks[i++] = cBlock;
            }
        }
    }

    // Sort the blocks from last used to the most recently used
    if (i > 1)
        textureQuickSort(activeBlocks, 0, i - 1, thread);

    // Free the memory
    if (all)
        CRenderer::textureMaxMemory[thread] = 0;
    for (j = 0; (j < i) && (CRenderer::textureUsedMemory[thread] > (CRenderer::textureMaxMemory[thread] / 2)); j++) {
        cBlock = activeBlocks[j];

        CRenderer::textureUsedMemory[thread] -= cBlock->size;
        cBlock->threadData[thread].data = NULL;

#ifdef TEXTURE_PERBLOCK_LOCK
        osLock(cBlock->mutex);
#endif

        if (--cBlock->refCount == 0) {
            stats.textureSize -= cBlock->size;
            stats.textureMemory -= cBlock->size;

            delete[] (unsigned char *)cBlock->data;
            cBlock->data = NULL;
        }

#ifdef TEXTURE_PERBLOCK_LOCK
        osUnlock(cBlock->mutex);
#endif
    }

    memEnd(context->threadMemory);

#ifdef TEXTURE_PERBLOCK_LOCK
    osUnlock(CRenderer::textureMutex);
#endif
}

///////////////////////////////////////////////////////////////////////
// Function				:	textureLoadBlock
// Description			:	Read a block of texture from disk
// Return Value			:	Pointer to the new texture
// Comments				:
static inline unsigned char *textureAllocateBlock(CTextureBlock *entry, CShadingContext *context) {
    stats.textureSize += entry->size;
    if (stats.peakTextureSize < stats.textureSize) {
        stats.peakTextureSize = stats.textureSize;
    }
    stats.textureMemory += entry->size;
    stats.transferredTextureData += entry->size;

    const int thread = context->thread;

    CRenderer::textureUsedMemory[thread] += entry->size;

    unsigned char *data = new unsigned char[entry->size];

    // If we exceeded the maximum texture memory, phase out the last texture
    if (CRenderer::textureUsedMemory[thread] > CRenderer::textureMaxMemory[thread])
        textureMemFlush(entry, context, FALSE);

    return data;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTiffTileSource
// Description			:	CTileSource backend for the baked, tiled/mipmapped
//							TIFF texture container (otexmake's output
//							format). A behavior-preserving extraction of
//							textureLoadBlock()'s prior inline TIFF I/O --
//							see spec 019 (019-tilesource-extraction). One
//							instance per mip level: "directory" identifies
//							which TIFF directory (mip level) this instance
//							reads.
// Comments				:	Thread safety matches the pre-refactor code
//							exactly: each fetchTile() opens its own fresh
//							TIFF handle (libtiff handles are not safe to
//							share across threads), used only for the
//							duration of that one call.
class CTiffTileSource : public CTileSource {
    public:
        CTiffTileSource(const char *name, short directory) {
            this->name = strdup(name);
            this->directory = directory;
        }

        virtual ~CTiffTileSource() {
            free(name);
        }

        void info(CTileLevelInfo &info) {
            TIFF *in = TIFFOpen(name, "r");
            if (in == NULL) {
                // Fail closed, per contract rule 5: leave info at its
                // member-initializer defaults rather than crash. assert()
                // alone isn't enough here -- it compiles out under NDEBUG
                // (a Release build), same trap this project's own
                // test_tile_source_tiff_info.cpp's CHECK macro exists to
                // avoid.
                return;
            }
            TIFFSetDirectory(in, directory);

            uint32_t width, height;
            uint16_t numSamples;
            uint16_t bitspersample;
            TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetFieldDefaulted(in, TIFFTAG_IMAGELENGTH, &height);
            TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &numSamples);
            TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);

            info.width = (int)width;
            info.height = (int)height;
            info.numChannels = numSamples;
            info.bitsPerSample = bitspersample;
            info.isFloatFormat = (bitspersample == 32);

            if (TIFFIsTiled(in)) {
                uint32_t tileWidth, tileHeight;
                TIFFGetFieldDefaulted(in, TIFFTAG_TILEWIDTH, &tileWidth);
                TIFFGetFieldDefaulted(in, TIFFTAG_TILELENGTH, &tileHeight);
                info.tileWidth = (int)tileWidth;
                info.tileHeight = (int)tileHeight;
            }
            else {
                // Untiled (flat/un-made) source -- the entire image is
                // the one implicit "tile", per contract rule 1.
                info.tileWidth = (int)width;
                info.tileHeight = (int)height;
            }

            TIFFClose(in);
        }

        bool fetchTile(int tileX, int tileY, void *dest) {
            // Note: that we are thread safe because each TIFFOpen returns a fresh
            // handle which we can operate on provided it's not used in any other thread
            // We don't set the error handler here, as it will have been set when we
            // loaded the texture.  Error handler installation invocation is the only
            // thread-unsafe part of libtiff.  It's important the innards of the handler
            // don't do anything which would be problematic if more than one thread
            // executed it

            // Open the file
            TIFF *in = TIFFOpen(name, "r");
            if (in == NULL) { // Error, we opened this file before
                              // The stupid user must have deleted the
                              // file or unmounted the drive while in progress
                return false;
            }

            TIFFSetDirectory(in, directory);

            // Get the texture properties
            // Note: using fileWidth, rather than the pixar full width is fine,
            // we only use this to work out whether we're tiled or not
            uint32_t width, height;
            uint16_t numSamples;
            uint16_t bitspersample;
            TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetFieldDefaulted(in, TIFFTAG_IMAGELENGTH, &height);
            TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &numSamples);
            TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
            int tiled = TIFFIsTiled(in);

            int bytesPerSample;
            int pixelSize;
            if (bitspersample == 8) {
                bytesPerSample = sizeof(unsigned char);
                pixelSize = numSamples * sizeof(unsigned char);
            }
            else if (bitspersample == 16) {
                bytesPerSample = sizeof(unsigned short);
                pixelSize = numSamples * sizeof(unsigned short);
            }
            else {
                assert(bitspersample == 32);
                bytesPerSample = sizeof(float);
                pixelSize = numSamples * sizeof(float);
            }

            // Reconstruct the region this fetch covers, exactly as the
            // two pre-refactor callers used to compute and pass in: a
            // full, tile-aligned tile for a tiled source, or the entire
            // image for an untiled one -- fetchTile() is never asked for
            // anything else (contract rule 2), so this always matches
            // what the old caller-supplied x/y/w/h were.
            int x, y, w, h;
            if (tiled) {
                uint32_t tileWidthTag, tileHeightTag;
                TIFFGetFieldDefaulted(in, TIFFTAG_TILEWIDTH, &tileWidthTag);
                TIFFGetFieldDefaulted(in, TIFFTAG_TILELENGTH, &tileHeightTag);
                w = (int)tileWidthTag;
                h = (int)tileHeightTag;
                x = tileX * w;
                y = tileY * h;
            }
            else {
                w = (int)width;
                h = (int)height;
                x = 0;
                y = 0;
            }

            void *data = dest;

            // Do we need to read the entire texture ?
            if ((x != 0) || (y != 0) || (w != (int)width) || (h != (int)height)) {
                // No , is the file tiled ?
                if (!tiled) {
                    // No, read the required portion
                    //
                    // Dead in practice: an untiled source is only ever
                    // requested as a whole (x=0,y=0,w=width,h=height, the
                    // derivation above), so this branch never executes
                    // for any current caller -- preserved verbatim per
                    // FR-005, with one deliberate adaptation: the scratch
                    // buffer below used to come from the calling shading
                    // context's per-thread arena (memBegin/ralloc/memEnd
                    // against context->threadMemory); fetchTile()
                    // deliberately takes no CShadingContext* (so future
                    // non-TIFF backends aren't coupled to shading-context
                    // internals), so a plain heap buffer replaces it here
                    // -- the read/copy logic itself is unchanged.
                    std::vector<unsigned char> tdata((size_t)width * height * pixelSize);

                    // Read the entire image
                    assert((int)(pixelSize * width) == TIFFScanlineSize(in));
                    for (int i = 0; i < (int)height; ++i) {
                        TIFFReadScanline(in, &tdata[pixelSize * i * width], i, 0);
                        if (i >= (y + h))
                            break; // If we read the last required scanline, break
                    }

                    for (int i = 0; i < h; ++i) {
                        memcpy(&((unsigned char *)data)[i * pixelSize * w], &tdata[((y + i) * width + x) * pixelSize], w * pixelSize);
                    }
                }
                else {
                    uint32_t tileWidth, tileHeight;

                    TIFFGetFieldDefaulted(in, TIFFTAG_TILEWIDTH, &tileWidth);
                    TIFFGetFieldDefaulted(in, TIFFTAG_TILELENGTH, &tileHeight);
                    assert(tileWidth == (uint32_t)w);
                    assert(tileHeight == (uint32_t)h);
                    assert((x % tileWidth) == 0);
                    assert((y % tileHeight) == 0);

                    unsigned short planarConfig;
                    TIFFGetFieldDefaulted(in, TIFFTAG_PLANARCONFIG, &planarConfig);

                    if (planarConfig == PLANARCONFIG_SEPARATE) {
                        // Dead in practice: otexmake never writes
                        // separate planar config. Preserved verbatim per
                        // FR-005.
                        unsigned char *buffer = (unsigned char *)alloca(bytesPerSample * tileWidth * tileHeight);
                        unsigned char *cdata = (unsigned char *)data;
                        for (int c = 0; c < numSamples; ++c) {
                            TIFFReadTile(in, buffer, x, y, 0, c);
                            for (unsigned int i = 0; i < tileWidth * tileHeight; ++i) {
                                memcpy(cdata + (i * numSamples + c) * bytesPerSample, buffer + i * bytesPerSample, bytesPerSample);
                            }
                        }
                    }
                    else {
                        TIFFReadTile(in, data, x, y, 0, 0);
                    }
                }
            }
            else {
                // We need to read the entire texture
                if (tiled) {
                    uint32_t tileWidth, tileHeight;

                    TIFFGetFieldDefaulted(in, TIFFTAG_TILEWIDTH, &tileWidth);
                    TIFFGetFieldDefaulted(in, TIFFTAG_TILELENGTH, &tileHeight);

                    if ((x != 0) || (y != 0) || (w != (int)tileWidth) || (h != (int)tileHeight)) {
                        error(CODE_BUG, "Tiled unmade texture\n");
                    }
                    else {
                        unsigned short planarConfig;
                        TIFFGetFieldDefaulted(in, TIFFTAG_PLANARCONFIG, &planarConfig);

                        if (planarConfig == PLANARCONFIG_SEPARATE) {
                            unsigned char *buffer = (unsigned char *)alloca(bytesPerSample * tileWidth * tileHeight);
                            unsigned char *cdata = (unsigned char *)data;
                            for (int c = 0; c < numSamples; ++c) {
                                TIFFReadTile(in, buffer, x, y, 0, c);
                                for (unsigned int i = 0; i < tileWidth * tileHeight; ++i) {
                                    memcpy(cdata + (i * numSamples + c) * bytesPerSample, buffer + i * bytesPerSample, bytesPerSample);
                                }
                            }
                        }
                        else {
                            TIFFReadTile(in, data, x, y, 0, 0);
                        }
                    }
                }
                else {
                    // Read the entire image
                    assert((int)(pixelSize * width) == TIFFScanlineSize(in));
                    for (int i = 0; i < (int)height; ++i) {
                        TIFFReadScanline(in, &((unsigned char *)data)[pixelSize * i * width], i, 0);
                    }
                }
            }

            TIFFClose(in);
            return true;
        }

    private:
        char *name;
        short directory;
};

CTileSource *createTiffTileSource(const char *filename, short directory) {
    return new CTiffTileSource(filename, directory);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSynthesizedPyramid
// Description			:	The full, decoded-once, immutable in-memory mip
//							pyramid built from an unbaked (non-TIFF)
//							texture source (spec 020,
//							020-runtime-tile-paging). One entry per level,
//							index 0 = full (post-resize) resolution.
//							Conceptually the in-memory counterpart to what
//							a baked TIFF's own directory structure already
//							provides -- see data-model.md.
// Comments				:	Shared (via std::shared_ptr) across every
//							per-level CSynthesizedTileSource view
//							constructed from it (research.md SS1). Built
//							exactly once, single-threaded, then never
//							mutated -- safe for concurrent reads from
//							multiple CSynthesizedTileSource instances with
//							no locking of its own.
class CSynthesizedPyramid {
    public:
        struct Level {
            int width = 0;
            int height = 0;
            std::vector<unsigned char> data; // tightly-packed, row-major
        };

        std::vector<Level> levels;
        int numChannels = 0;
        int bitsPerSample = 0; // 8, 16, or 32
        bool isFloatFormat = false;

        // The ratio-preserving target width/height adjustSize<T> computed
        // BEFORE padding level 0 up to a power of two (its own
        // "validWidth"/"validHeight" out-parameters) -- the in-memory
        // counterpart to TIFFTAG_PIXAR_IMAGEFULLWIDTH/LENGTH, which
        // texLoad()/readMadeTexture() read in preference to the padded
        // TIFFTAG_IMAGEWIDTH/LENGTH "so we can rescale when otexmake
        // makes a texture power of 2 but retains aspect ratio" (that
        // existing comment, texture.cpp:2569-2570). For an
        // already-power-of-two source these equal levels[0].width/height.
        int validWidth = 0;
        int validHeight = 0;
};

///////////////////////////////////////////////////////////////////////
// Function				:	reduceSynthesizedPyramidLevel
// Description			:	Reduces one pyramid level into the next,
//							in-place over the arena-allocated buffer
//							"data" (which is sized for the base level and
//							therefore always big enough for every smaller
//							level), replicating appendPyramid<T>()'s exact
//							2x2-block-average math (texmake.cpp:200-273)
//							without writing to a TIFF file.
// Return Value			:	-
// Comments				:	Must be called with CRenderer::synthesizeMutex
//							held and inside the same memBegin/memEnd
//							bracket as the caller's arena allocations
//							(research.md SS4a) -- "fnextLevel" is itself
//							arena-allocated.
template <class T>
static void reduceSynthesizedPyramidLevel(T *data, int currentWidth, int numSamples, int nextWidth, int nextHeight, float *fnextLevel) {
    for (int y = 0, yo = 0; y < nextHeight; y++, yo += 2) {
        T *src = &data[yo * currentWidth * numSamples];
        float *dest = &fnextLevel[y * nextWidth * numSamples];

        for (int x = 0; x < nextWidth; x++) {
            for (int n = 0; n < numSamples; n++) {
                dest[n] = src[n];
                dest[n] += src[numSamples + n];
            }
            dest += numSamples;
            src += 2 * numSamples;
        }

        src = &data[(yo + 1) * currentWidth * numSamples];
        dest = &fnextLevel[y * nextWidth * numSamples];

        for (int x = 0; x < nextWidth; x++) {
            for (int n = 0; n < numSamples; n++) {
                dest[n] += src[n];
                dest[n] += src[numSamples + n];
            }
            dest += numSamples;
            src += 2 * numSamples;
        }

        dest = &fnextLevel[y * nextWidth * numSamples];
        for (int x = 0; x < nextWidth * numSamples; x++) {
            *dest++ *= 1 / (float)4;
        }
    }

    for (int s = 0; s < nextWidth * nextHeight * numSamples; s++) {
        data[s] = (T)fnextLevel[s];
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	buildSynthesizedPyramidTyped
// Description			:	Resizes a decoded source up to a power of two
//							if needed (reusing the relocated
//							adjustSize<T>/filterScaleImage<T>, T006), then
//							builds every mip level by repeated 2x2-block-
//							average reduction, copying each level's result
//							out of the shared arena into the pyramid's own
//							heap-owned buffers.
// Return Value			:	-
// Comments				:	Must be called with CRenderer::synthesizeMutex
//							held and inside the caller's memBegin/memEnd
//							bracket (research.md SS4a) -- "decodedData" is
//							itself arena-allocated by the caller.
template <class T>
static void buildSynthesizedPyramidTyped(const CImageInfo &info, void *decodedData, CSynthesizedPyramid *pyramid) {
    int width = info.width;
    int height = info.height;
    int numSamples = info.numChannels;
    int bitspersample = info.bitsPerSample;
    T *data = (T *)decodedData;
    int validWidth, validHeight;

    // Non-power-of-two resize, matching otexmake's/makeTexture()'s own
    // actual default: "up" resize mode, periodic/periodic wrap,
    // RiCatmullRomFilter at width/height 3.0 (research.md SS3).
    // "up": otexmake's/makeTexture()'s own actual default resize mode
    // (resizeUpMode = "up" in texmake.cpp, not reachable here since it
    // wasn't part of T006's relocation -- see research.md SS3's
    // correction). Any string that isn't "down"/"round"/"none" produces
    // the same round-up-to-power-of-two behavior in adjustSize<T>.
    adjustSize<T>(&data, &width, &height, &validWidth, &validHeight, numSamples, bitspersample, 3.0f, 3.0f, RiCatmullRomFilter, RI_PERIODIC, RI_PERIODIC, "up");

    pyramid->validWidth = validWidth;
    pyramid->validHeight = validHeight;

    int numLevels = tiffNumLevels(width, height);
    pyramid->levels.resize(numLevels);

    size_t level0Bytes = (size_t)width * height * numSamples * sizeof(T);
    pyramid->levels[0].width = width;
    pyramid->levels[0].height = height;
    pyramid->levels[0].data.resize(level0Bytes);
    memcpy(pyramid->levels[0].data.data(), data, level0Bytes);

    if (numLevels > 1) {
        float *fnextLevel = (float *)ralloc((size_t)width * height * numSamples * sizeof(float), CRenderer::globalMemory);
        int currentWidth = width;
        int currentHeight = height;

        for (int lvl = 1; lvl < numLevels; lvl++) {
            int nextWidth = currentWidth >> 1;
            int nextHeight = currentHeight >> 1;

            reduceSynthesizedPyramidLevel<T>(data, currentWidth, numSamples, nextWidth, nextHeight, fnextLevel);

            currentWidth = nextWidth;
            currentHeight = nextHeight;

            size_t levelBytes = (size_t)currentWidth * currentHeight * numSamples * sizeof(T);
            pyramid->levels[lvl].width = currentWidth;
            pyramid->levels[lvl].height = currentHeight;
            pyramid->levels[lvl].data.resize(levelBytes);
            memcpy(pyramid->levels[lvl].data.data(), data, levelBytes);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	buildSynthesizedPyramid
// Description			:	Decodes filename via CImageInput and builds the
//							full in-memory mip pyramid described above.
//							Returns nullptr if filename cannot be decoded
//							by any supported CImageInput backend, or on
//							any decode failure.
// Return Value			:	std::shared_ptr<CSynthesizedPyramid>, or an
//							empty shared_ptr on failure
// Comments				:	Serializes its entire decode+resize+reduction
//							body behind CRenderer::synthesizeMutex, because
//							adjustSize<T>/filterScaleImage<T>/filterImage<T>
//							(and this function's own reduction step) share
//							CRenderer::globalMemory, an unsynchronized bump
//							allocator, and this function is reachable from
//							multiple shading threads concurrently via
//							CRenderer::textureLoad() (research.md SS4a).
//							Not on the hot per-tile fetchTile() path -- this
//							runs once per distinct unbaked source per
//							render.
static std::shared_ptr<CSynthesizedPyramid> buildSynthesizedPyramid(const char *filename) {
    std::unique_ptr<CImageInput> input(createImageInput(filename));
    if (input.get() == NULL) {
        return std::shared_ptr<CSynthesizedPyramid>();
    }

    CImageInfo info;
    if (!input->open(filename, info)) {
        return std::shared_ptr<CSynthesizedPyramid>();
    }

    std::shared_ptr<CSynthesizedPyramid> pyramid = std::make_shared<CSynthesizedPyramid>();
    pyramid->numChannels = info.numChannels;
    pyramid->bitsPerSample = info.bitsPerSample;
    pyramid->isFloatFormat = info.isFloatFormat;

    // readOk is declared outside the memBegin/memEnd bracket below on
    // purpose: memBegin/memEnd are brace-matched macros (memory.h), so
    // anything declared between them goes out of scope at memEnd -- it
    // must be read only after that scope closes.
    bool readOk;

    osLock(CRenderer::synthesizeMutex);
    memBegin(CRenderer::globalMemory);

    int bytesPerSample = (info.bitsPerSample == 8) ? (int)sizeof(unsigned char) : (info.bitsPerSample == 16) ? (int)sizeof(unsigned short)
                                                                                                              : (int)sizeof(float);
    size_t decodeSize = (size_t)info.width * info.height * info.numChannels * bytesPerSample;
    void *decodedData = ralloc(decodeSize, CRenderer::globalMemory);

    readOk = input->readImage(decodedData);

    if (readOk) {
        if (info.bitsPerSample == 8) {
            buildSynthesizedPyramidTyped<unsigned char>(info, decodedData, pyramid.get());
        }
        else if (info.bitsPerSample == 16) {
            buildSynthesizedPyramidTyped<unsigned short>(info, decodedData, pyramid.get());
        }
        else {
            assert(info.bitsPerSample == 32);
            buildSynthesizedPyramidTyped<float>(info, decodedData, pyramid.get());
        }
    }

    memEnd(CRenderer::globalMemory);
    osUnlock(CRenderer::synthesizeMutex);

    input->close();

    if (!readOk) {
        return std::shared_ptr<CSynthesizedPyramid>();
    }

    return pyramid;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSynthesizedTileSource
// Description			:	CTileSource backend for a runtime-synthesized,
//							in-memory mip pyramid built from a non-TIFF
//							(unbaked) texture source (spec 020,
//							020-runtime-tile-paging). One instance per mip
//							level, each holding a
//							std::shared_ptr<CSynthesizedPyramid> into the
//							same shared structure (research.md SS1) --
//							see contracts/synthesized-tile-source.md.
// Comments				:	Safe for concurrent fetchTile() calls across
//							different instances: the pyramid it reads from
//							is built once, single-threaded (behind
//							buildSynthesizedPyramid()'s
//							CRenderer::synthesizeMutex), before any
//							concurrent fetchTile() call can happen, and
//							never mutated afterward. Uses
//							DEFAULT_TILE_SIZE-square tiles, matching
//							otexmake's own default (research.md SS2);
//							partial trailing tiles copy only the valid,
//							in-bounds portion, same as libtiff's own
//							tiled-image API already does for
//							CTiffTileSource's smallest mip levels -- no new
//							zero-fill/padding logic.
class CSynthesizedTileSource : public CTileSource {
    public:
        CSynthesizedTileSource(std::shared_ptr<CSynthesizedPyramid> pyramid, int level) : pyramid(pyramid), level(level) {}

        virtual ~CSynthesizedTileSource() {}

        void info(CTileLevelInfo &info) {
            const CSynthesizedPyramid::Level &lvl = pyramid->levels[level];

            info.width = lvl.width;
            info.height = lvl.height;
            // DEFAULT_TILE_SIZE unconditionally, matching the baked-TIFF
            // convention (appendLayer(), texmake.cpp): every level uses
            // the SAME fixed tile size regardless of that level's own
            // width/height, so a level smaller than DEFAULT_TILE_SIZE is
            // exactly one (partially-valid) tile -- NOT clamped down to
            // the level's own dimensions. CTiledTexture<T>'s
            // tileWidthShift/tileHeightShift computation (texture.cpp,
            // readMadeTexture()) assumes a fixed power-of-two tile size
            // across all levels of a texture; a per-level-shrunk value
            // would not generally be a power of two and would break that
            // computation.
            info.tileWidth = DEFAULT_TILE_SIZE;
            info.tileHeight = DEFAULT_TILE_SIZE;
            info.numChannels = pyramid->numChannels;
            info.bitsPerSample = pyramid->bitsPerSample;
            info.isFloatFormat = pyramid->isFloatFormat;
        }

        bool fetchTile(int tileX, int tileY, void *dest) {
            const CSynthesizedPyramid::Level &lvl = pyramid->levels[level];

            const int tileWidth = DEFAULT_TILE_SIZE;
            const int tileHeight = DEFAULT_TILE_SIZE;

            const int bytesPerSample = (pyramid->bitsPerSample == 8) ? (int)sizeof(unsigned char) : (pyramid->bitsPerSample == 16) ? (int)sizeof(unsigned short)
                                                                                                                                     : (int)sizeof(float);
            const int pixelSize = pyramid->numChannels * bytesPerSample;

            const int x0 = tileX * tileWidth;
            const int y0 = tileY * tileHeight;

            if ((x0 < 0) || (y0 < 0) || (x0 >= lvl.width) || (y0 >= lvl.height)) {
                // Contract rule 5: fail closed only for a request entirely
                // outside the level's valid bounds -- a caller bug, since
                // every real caller already clamps coordinates.
                return false;
            }

            const int validWidth = std::min(tileWidth, lvl.width - x0);
            const int validHeight = std::min(tileHeight, lvl.height - y0);

            const unsigned char *src = lvl.data.data();
            unsigned char *dst = (unsigned char *)dest;

            for (int row = 0; row < validHeight; row++) {
                memcpy(dst + (size_t)row * tileWidth * pixelSize, src + ((size_t)(y0 + row) * lvl.width + x0) * pixelSize, (size_t)validWidth * pixelSize);
            }

            return true;
        }

    private:
        std::shared_ptr<CSynthesizedPyramid> pyramid;
        int level;
};

CTileSource *createSynthesizedTileSource(const char *filename, int level) {
    std::shared_ptr<CSynthesizedPyramid> pyramid = buildSynthesizedPyramid(filename);
    if (!pyramid || (level < 0) || (level >= (int)pyramid->levels.size())) {
        return NULL;
    }

    return new CSynthesizedTileSource(pyramid, level);
}

///////////////////////////////////////////////////////////////////////
// Function				:	textureLoadBlock
// Description			:	Read a block of texture from disk
// Return Value			:	Pointer to the new texture
// Comments				:
static inline void textureLoadBlock(CTextureBlock *entry, CTileSource *source, int tileX, int tileY, CShadingContext *context) {

    osLock(entry->mutex);

    // if we already have the data, use that and increment the reference count
    if (entry->data != NULL) {
        entry->threadData[context->thread].data = entry->data;
        entry->refCount++;

        osUnlock(entry->mutex);
        return;
    }

    // Update the state
    stats.numTextureMisses++;

    // Allocate space for the texture
    assert(entry->data == NULL);
    void *data = textureAllocateBlock(entry, context);

    if (!source->fetchTile(tileX, tileY, data)) {
        // FIXME: Is this an error ?
        //
        // Matches the pre-refactor outcome: a failed fetch (e.g. the
        // underlying file deleted/unmounted mid-render, per spec.md's Edge
        // Cases) must not leave entry->data pointing at an
        // uninitialized/garbage-filled buffer for a lookupPixel() access
        // macro to silently read -- free it and leave entry->data NULL,
        // exactly as it stayed NULL when the pre-refactor code's TIFFOpen
        // failed (this refactor does now run textureAllocateBlock()'s
        // stats bookkeeping even in this failure case, a narrow,
        // deliberate accounting difference from before).
        delete[] (unsigned char *)data;
        data = NULL;
    }

    // See note below about out of order architectures.  The functions above take care of this

    assert(entry->refCount == 0);

    entry->refCount = 1;
    entry->data = data;
    entry->threadData[context->thread].data = data;

    osUnlock(entry->mutex);
}

//////////////////////////////////////////////////////////////////////
// Function				:	texturRegisterBlock
// Description			:	Add a block into the list of used blocks
// Return Value			:	Pointer to the new block
// Comments				:
static inline void textureRegisterBlock(CTextureBlock *cEntry, int size) {

    // Fully construct the cEntry before placing it on the list
    cEntry->data = NULL;
    cEntry->refCount = 0;
    cEntry->threadData = new CTexBlockThreadData[CRenderer::numThreads];
    cEntry->size = size;

#ifdef TEXTURE_PERBLOCK_LOCK
    osCreateMutex(cEntry->mutex);
#endif
    for (int i = 0; i < CRenderer::numThreads; ++i) {
        cEntry->threadData[i].data = NULL;
        cEntry->threadData[i].lastRefNumber = 0; // FIXME: is this right?
                                                 // should be CRenderer::textureRefNumber ???
    }

    // Place cEntry on list
    cEntry->prev = NULL;
    cEntry->next = CRenderer::textureUsedBlocks;
    if (CRenderer::textureUsedBlocks != NULL)
        CRenderer::textureUsedBlocks->prev = cEntry;
    CRenderer::textureUsedBlocks = cEntry;
}

//////////////////////////////////////////////////////////////////////
// Function				:	texturUnregisterBlock
// Description			:	Remove the block
// Return Value			:	Pointer to the new block
// Comments				:
static inline void textureUnregisterBlock(CTextureBlock *cEntry) {

    if (cEntry->next != NULL)
        cEntry->next->prev = cEntry->prev;
    if (cEntry->prev != NULL)
        cEntry->prev->next = cEntry->next;
    else
        CRenderer::textureUsedBlocks = cEntry->next;

    // assert(cEntry->refCount == 0);//? bad idea?

    if (cEntry->data != NULL) {
        stats.textureSize -= cEntry->size;
        stats.textureMemory -= cEntry->size;
        // how to deal with this??
        for (int i = 0; i < CRenderer::numThreads; ++i) {
            if (cEntry->threadData[i].data != NULL) {
                CRenderer::textureUsedMemory[i] -= cEntry->size;
            }
        }
        delete[] (unsigned char *)cEntry->data;
    }

    delete[] cEntry->threadData;

#ifdef TEXTURE_PERBLOCK_LOCK
    osDeleteMutex(cEntry->mutex);
#endif
}

///////////////////////////////////////////////////////////////////////
//
//		Class declarations
//
///////////////////////////////////////////////////////////////////////

// Texture wrapping modes
typedef enum {
    TEXTURE_PERIODIC = 0,
    TEXTURE_BLACK,
    TEXTURE_CLAMP
} TTextureMode;

///////////////////////////////////////////////////////////////////////
// Class				:	CTextureLayer
// Description			:	This class encapsulates a single 2D texture layer in a file
// Comments				:
class CTextureLayer {
    public:
        CTextureLayer(CTileSource *tileSource, int width, int height, short numSamples, int fileWidth, int fileHeight, TTextureMode sMode, TTextureMode tMode) {
            this->tileSource = tileSource;
            this->width = width;
            this->height = height;
            this->numSamples = numSamples;
            this->fileWidth = fileWidth;
            this->fileHeight = fileHeight;
            this->sMode = sMode;
            this->tMode = tMode;
        }

        virtual ~CTextureLayer() {
            delete tileSource;
        }

        void lookup(float *r, float s, float t, CShadingContext *context) {
            s *= width; // To the pixel space
            t *= height;
            s -= 0.5; // Pixel centers
            t -= 0.5;
            int si = (int)floor(s); // The integer pixel coordinates
            int ti = (int)floor(t);
            float ds = s - si;
            float dt = t - ti;

            if (si >= width)
                si = (sMode == TEXTURE_PERIODIC) ? (si - width) : (width - 1);
            if (ti >= height)
                ti = (tMode == TEXTURE_PERIODIC) ? (ti - height) : (height - 1);

            float res[4 * 3];
            lookupPixel(res, si, ti, context);

            float tmp;

            tmp = (1 - ds) * (1 - dt); // Bilinear interpolation
            r[0] = res[0] * tmp;
            r[1] = res[1] * tmp;
            r[2] = res[2] * tmp;

            tmp = ds * (1 - dt);
            r[0] += res[3] * tmp;
            r[1] += res[4] * tmp;
            r[2] += res[5] * tmp;

            tmp = (1 - ds) * dt;
            r[0] += res[6] * tmp;
            r[1] += res[7] * tmp;
            r[2] += res[8] * tmp;

            tmp = ds * dt;
            r[0] += res[9] * tmp;
            r[1] += res[10] * tmp;
            r[2] += res[11] * tmp;
        }

        float lookupz(float s, float t, float z, CShadingContext *context) {
            s *= width; // To the pixel space
            t *= height;
            s -= 0.5; // Pixel centers
            t -= 0.5;
            int si = (int)floor(s); // The integer pixel coordinates
            int ti = (int)floor(t);
            float ds = s - si;
            float dt = t - ti;

            if (si >= width)
                si = (sMode == TEXTURE_PERIODIC) ? (si - width) : (width - 1);
            if (ti >= height)
                ti = (tMode == TEXTURE_PERIODIC) ? (ti - height) : (height - 1);

            float res[4 * 3];
            lookupPixel(res, si, ti, context);

            float r = 0;

            if (z > res[0]) {
                r += (1 - ds) * (1 - dt);
            }

            if (z > res[3]) {
                r += ds * (1 - dt);
            }

            if (z > res[6]) {
                r += (1 - ds) * dt;
            }

            if (z > res[9]) {
                r += ds * dt;
            }

            return r;
        }

        CTileSource *tileSource;   // Owned; fetches this layer's pixel data
        short numSamples;          // The number of samples in the texture
        int width, height;         // The image info
        int fileWidth, fileHeight; // The physical size in the file
        TTextureMode sMode, tMode; // The wrap modes
    protected:
        // This function must be overriden by the child class
        virtual void lookupPixel(float *, int, int, CShadingContext *context) = 0; // Lookup 4 pixel values
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBasicTexture
// Description			:	Thic class holds a basic uncached texture
// Comments				:
template <class T>
class CBasicTexture : public CTextureLayer {
    public:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CBasicTexture
        // Method				:	CBasicTexture
        // Description			:	Ctor
        // Return Value			:	-
        // Comments				:
        CBasicTexture(CTileSource *tileSource, int width, int height, short numSamples, int fileWidth, int fileHeight, TTextureMode sMode, TTextureMode tMode, double Mult) : CTextureLayer(tileSource, width, height, numSamples, fileWidth, fileHeight, sMode, tMode) {
            textureRegisterBlock(&dataBlock, width * height * numSamples * sizeof(T));
            M = Mult;
        }

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CBasicTexture
        // Method				:	~CBasicTexture
        // Description			:	Dtor
        // Return Value			:	-
        // Comments				:
        ~CBasicTexture() {
            textureUnregisterBlock(&dataBlock);
        }

    protected:
        // The pixel lookup
        void lookupPixel(float *res, int x, int y, CShadingContext *context) {

            const int thread = context->thread;

            if (dataBlock.threadData[thread].data == NULL) {
                // The data is cached out
                textureLoadBlock(&dataBlock, tileSource, 0, 0, context);
            }

            // Texture cache management
            (*CRenderer::textureRefNumber[thread])++;
            dataBlock.threadData[thread].lastRefNumber = *CRenderer::textureRefNumber[thread];

            int xi = x + 1;
            int yi = y + 1;

            // these must be after the xi,yi calculation
            if (x < 0)
                x = (sMode == TEXTURE_PERIODIC) ? (x + width) : 0;
            if (y < 0)
                y = (tMode == TEXTURE_PERIODIC) ? (y + height) : 0;
            if (xi >= width)
                xi = (sMode == TEXTURE_PERIODIC) ? (xi - width) : (width - 1);
            if (yi >= height)
                yi = (tMode == TEXTURE_PERIODIC) ? (yi - height) : (height - 1);

            const T *data;

#define access(__x, __y)                                               \
    data = (T *)dataBlock.data + (__y * fileWidth + __x) * numSamples; \
    res[0] = (float)(data[0] * M);                                     \
    res[1] = (float)(data[1] * M);                                     \
    res[2] = (float)(data[2] * M);                                     \
    res += 3;

            access(x, y);
            access(xi, y);
            access(x, yi);
            access(xi, yi);
#undef access
        }

    private:
        CTextureBlock dataBlock;
        double M;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTiledTexture
// Description			:	Thi class holds info about a tiled texture (tiled textures are cached in tiles)
// Comments				:
template <class T>
class CTiledTexture : public CTextureLayer {
    public:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CTiledTexture
        // Method				:	CTiledTexture
        // Description			:	Ctor
        // Return Value			:	-
        // Comments				:
        CTiledTexture(CTileSource *tileSource, int width, int height, short numSamples, int fileWidth, int fileHeight, TTextureMode sMode, TTextureMode tMode, int tileWidth, int tileWidthShift, int tileHeight, int tileHeightShift, double Mult) : CTextureLayer(tileSource, width, height, numSamples, fileWidth, fileHeight, sMode, tMode) {
            this->tileWidth = tileWidth;
            this->tileWidthShift = tileWidthShift;
            this->tileHeight = tileHeight;
            this->tileHeightShift = tileHeightShift;

            xTiles = (int)ceil((float)width / (float)tileWidth);
            yTiles = (int)ceil((float)height / (float)tileHeight);

            const int tileLength = tileWidth * tileHeight * numSamples * sizeof(T);

            dataBlocks = new CTextureBlock *[yTiles];
            for (int i = 0; i < yTiles; ++i) {
                dataBlocks[i] = new CTextureBlock[xTiles];

                for (int j = 0; j < xTiles; ++j) {
                    textureRegisterBlock(dataBlocks[i] + j, tileLength);
                }
            }

            M = Mult;
        }

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CTiledTexture
        // Method				:	~CTiledTexture
        // Description			:	Dtor
        // Return Value			:	-
        // Comments				:
        ~CTiledTexture() {
            for (int i = 0; i < yTiles; ++i) {
                for (int j = 0; j < xTiles; ++j) {
                    textureUnregisterBlock(&dataBlocks[i][j]);
                }

                delete[] dataBlocks[i];
            }

            delete[] dataBlocks;
        }

    protected:
        // Pixel lookup
        void lookupPixel(float *res, int x, int y, CShadingContext *context) {
            int xTile;
            int yTile;
            CTextureBlock *block;
            const T *data;
            const int xt = tileWidth - 1;
            const int yt = tileHeight - 1;

            int xi = x + 1;
            int yi = y + 1;

            // these must be after the xi,yi calculation
            // x,y are relative to this layer
            if (x < 0)
                x = (sMode == TEXTURE_PERIODIC) ? (x + width) : 0;
            if (y < 0)
                y = (tMode == TEXTURE_PERIODIC) ? (y + height) : 0;
            if (xi >= width)
                xi = (sMode == TEXTURE_PERIODIC) ? (xi - width) : (width - 1);
            if (yi >= height)
                yi = (tMode == TEXTURE_PERIODIC) ? (yi - height) : (height - 1);

            const int thread = context->thread;

#define access(__x, __y)                                                                                                             \
    xTile = __x >> tileWidthShift;                                                                                                   \
    yTile = __y >> tileHeightShift;                                                                                                  \
    block = dataBlocks[yTile] + xTile;                                                                                               \
                                                                                                                                     \
    if (block->threadData[thread].data == NULL) {                                                                                    \
        textureLoadBlock(block, tileSource, xTile, yTile, context);                                                                  \
    }                                                                                                                                \
    assert(block->data != NULL);                                                                                                     \
    (*CRenderer::textureRefNumber[thread])++;                                                                                        \
    block->threadData[thread].lastRefNumber = *CRenderer::textureRefNumber[thread];                                                  \
                                                                                                                                     \
    data = (T *)block->data + (((__y & yt)) * tileWidth + (__x & xt)) * numSamples;                                                  \
    res[0] = (float)(data[0] * M);                                                                                                   \
    res[1] = (float)(data[1] * M);                                                                                                   \
    res[2] = (float)(data[2] * M);                                                                                                   \
    res += 3;

            access(x, y);
            access(xi, y);
            access(x, yi);
            access(xi, yi);
#undef access
        }

        CTextureBlock **dataBlocks;
        int xTiles, yTiles;
        int tileWidth, tileWidthShift;
        int tileHeight, tileHeightShift;
        double M;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CMadeTexture
// Description			:	A previously made texture
// Comments				:
class CMadeTexture : public CTexture {
    public:
        CMadeTexture(const char *name) : CTexture(name) {
            numLayers = 0;
            layers = NULL;
        }

        virtual ~CMadeTexture() {
            if (layers != NULL) {
                for (int i = 0; i < numLayers; ++i)
                    delete layers[i];
                delete[] layers;
            }
        }

        float lookupz(float s, float t, float z, CShadingContext *context) {
            assert(numLayers > 0);
            return layers[0]->lookupz(s, t, z, context);
        }

        void lookup(float *result, float s, float t, CShadingContext *context) {
            const float fill = context->currentShadingState->scratch.textureParams.fill;

            // Do the s mode
            switch (layers[0]->sMode) {
                case TEXTURE_PERIODIC:
                    s = fmodf(s, 1);
                    if (s < 0)
                        s += 1;
                    break;
                case TEXTURE_BLACK:
                    if ((s < 0) || (s > 1)) {
                        initv(result, fill);
                        return;
                    }
                    break;
                case TEXTURE_CLAMP:
                    if (s < 0)
                        s = 0;
                    if (s > 1)
                        s = 1;
                    break;
            }

            // Do the t mode
            switch (layers[0]->tMode) {
                case TEXTURE_PERIODIC:
                    t = fmodf(t, 1);
                    if (t < 0)
                        t += 1;
                    break;
                case TEXTURE_BLACK:
                    if ((t < 0) || (t > 1)) {
                        initv(result, fill);
                        return;
                    }
                    break;
                case TEXTURE_CLAMP:
                    if (t < 0)
                        t = 0;
                    if (t > 1)
                        t = 1;
                    break;
            }

            layers[0]->lookup(result, s, t, context);
        }

        void lookup4(float *result, const float *u, const float *v, CShadingContext *context) {
            int i;
            float totalContribution = 0;
            CTextureLayer *layer0, *layer1;
            float offset;
            float l;
            float diag;
            const float cs = ((u[0] + u[1] + u[2] + u[3]) * 0.25f);
            const float ct = ((v[0] + v[1] + v[2] + v[3]) * 0.25f);
            float ds, dt, d;
            const CShadingScratch *scratch = &(context->currentShadingState->scratch);

            const int width = layers[0]->width;
            const int height = layers[0]->height;

            // Compute the tri-linear level
            ds = (u[0] - cs) * width;
            dt = (v[0] - ct) * height;
            diag = ds * ds + dt * dt;

            ds = (u[1] - cs) * width;
            dt = (v[1] - ct) * height;
            d = ds * ds + dt * dt;
            if (diag > d) {
                diag = d;
            }

            ds = (u[2] - cs) * width;
            dt = (v[2] - ct) * height;
            d = ds * ds + dt * dt;
            if (diag > d) {
                diag = d;
            }

            ds = (u[3] - cs) * width;
            dt = (v[3] - ct) * height;
            d = ds * ds + dt * dt;
            if (diag > d) {
                diag = d;
            }

            diag += scratch->textureParams.blur * scratch->textureParams.blur * width * height;

            // Find the layer that we want to probe
            l = (logf(diag) * 0.5f * (1 / logf(2.0f)));
            if (0 > l) {
                l = 0;
            }

            i = (int)floor(l);
            if (i >= (numLayers - 1))
                i = numLayers - 2;

            layer0 = layers[i];
            layer1 = layers[i + 1];
            offset = l - i;
            if (1 < offset) {
                offset = 1;
            }

            const float jitter = 1.0f - 1.0f / (float)scratch->textureParams.samples;

            initv(result, 0, 0, 0); // Result is black
            for (i = (int)scratch->textureParams.samples; i > 0; i--) {
                float r[2];
                float s, t;

                context->random2d.get(r);

                // stratify the sample so that with low sample counts we don't jitter too much
                const float x = (r[0] - 0.5f) * jitter + 0.5f;
                const float y = (r[1] - 0.5f) * jitter + 0.5f;

                s = (u[0] * (1 - x) + u[1] * x) * (1 - y) +
                    (u[2] * (1 - x) + u[3] * x) * y;
                t = (v[0] * (1 - x) + v[1] * x) * (1 - y) +
                    (v[2] * (1 - x) + v[3] * x) * y;

                const float contribution = scratch->textureParams.filter(x - 0.5f, y - 0.5f, 1, 1);
                totalContribution += contribution;

                // Do the s mode
                switch (layers[0]->sMode) {
                    case TEXTURE_PERIODIC:
                        s = fmodf(s, 1);
                        if (s < 0)
                            s += 1;
                        break;
                    case TEXTURE_BLACK:
                        if ((s < 0) || (s > 1)) {
                            continue;
                        }
                        break;
                    case TEXTURE_CLAMP:
                        if (s < 0)
                            s = 0;
                        if (s > 1)
                            s = 1;
                        break;
                }

                // Do the t mode
                switch (layers[0]->tMode) {
                    case TEXTURE_PERIODIC:
                        t = fmodf(t, 1);
                        if (t < 0)
                            t += 1;
                        break;
                    case TEXTURE_BLACK:
                        if ((t < 0) || (t > 1)) {
                            continue;
                        }
                        break;
                    case TEXTURE_CLAMP:
                        if (t < 0)
                            t = 0;
                        if (t > 1)
                            t = 1;
                        break;
                }

                // lookup (s,t) and add it to the result
                vector C, CC0, CC1;
                layer0->lookup(CC0, s, t, context);
                layer1->lookup(CC1, s, t, context);
                interpolatev(C, CC0, CC1, offset);

                result[0] += C[0] * contribution;
                result[1] += C[1] * contribution;
                result[2] += C[2] * contribution;
            }

            const float tmp = 1 / totalContribution;
            mulvf(result, tmp);
        }

        // textureinfo support
        void getResolution(float *r) {
            r[0] = (float)layers[0]->width;
            r[1] = (float)layers[0]->height;
        }
        int getNumChannels() { return layers[0]->numSamples; }

        short numLayers;        // The number of layers
        CTextureLayer **layers; // The actual layers
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRegularTexture
// Description			:	A regular texture
// Comments				:
class CRegularTexture : public CTexture {
    public:
        CRegularTexture(const char *name) : CTexture(name) {
            layer = NULL;
        }

        virtual ~CRegularTexture() {
            if (layer != NULL)
                delete layer;
        }

        float lookupz(float s, float t, float z, CShadingContext *context) {
            assert(layer != NULL);
            return layer->lookupz(s, t, z, context);
        }

        void lookup(float *result, float s, float t, CShadingContext *context) {
            const float fill = context->currentShadingState->scratch.textureParams.fill;

            // Do the s mode
            switch (layer->sMode) {
                case TEXTURE_PERIODIC:
                    s = fmodf(s, 1);
                    if (s < 0)
                        s += 1;
                    break;
                case TEXTURE_BLACK:
                    if ((s < 0) || (s > 1)) {
                        initv(result, fill);
                        return;
                    }
                    break;
                case TEXTURE_CLAMP:
                    if (s < 0)
                        s = 0;
                    if (s > 1)
                        s = 1;
                    break;
            }

            // Do the t mode
            switch (layer->tMode) {
                case TEXTURE_PERIODIC:
                    t = fmodf(t, 1);
                    if (t < 0)
                        t += 1;
                    break;
                case TEXTURE_BLACK:
                    if ((t < 0) || (t > 1)) {
                        initv(result, fill);
                        return;
                    }
                    break;
                case TEXTURE_CLAMP:
                    if (t < 0)
                        t = 0;
                    if (t > 1)
                        t = 1;
                    break;
            }

            layer->lookup(result, s, t, context);
        }

        void lookup4(float *result, const float *u, const float *v, CShadingContext *context) {
            int i;
            float totalContribution = 0;
            const CShadingScratch *scratch = &(context->currentShadingState->scratch);

            const float jitter = 1.0f - 1.0f / (float)scratch->textureParams.samples;

            initv(result, 0, 0, 0); // Result is black
            for (i = (int)scratch->textureParams.samples; i > 0; i--) {
                float s, t;
                float r[2];

                context->random2d.get(r);

                // stratify the sample so that with low sample counts we don't jitter too much
                const float x = (r[0] - 0.5f) * jitter + 0.5f;
                const float y = (r[1] - 0.5f) * jitter + 0.5f;

                s = (u[0] * (1 - x) + u[1] * x) * (1 - y) +
                    (u[2] * (1 - x) + u[3] * x) * y;
                t = (v[0] * (1 - x) + v[1] * x) * (1 - y) +
                    (v[2] * (1 - x) + v[3] * x) * y;

                const float contribution = scratch->textureParams.filter(x - 0.5f, y - 0.5f, 1, 1);
                totalContribution += contribution;

                if (scratch->textureParams.blur > 0) {
                    context->random2d.get(r);

                    s += scratch->textureParams.blur * (x - 0.5f);
                    t += scratch->textureParams.blur * (y - 0.5f);
                }

                // Do the s mode
                switch (layer->sMode) {
                    case TEXTURE_PERIODIC:
                        s = fmodf(s, 1);
                        if (s < 0)
                            s += 1;
                        break;
                    case TEXTURE_BLACK:
                        if ((s < 0) || (s > 1)) {
                            continue;
                        }
                        break;
                    case TEXTURE_CLAMP:
                        if (s < 0)
                            s = 0;
                        if (s > 1)
                            s = 1;
                        break;
                }

                // Do the t mode
                switch (layer->tMode) {
                    case TEXTURE_PERIODIC:
                        t = fmodf(t, 1);
                        if (t < 0)
                            t += 1;
                        break;
                    case TEXTURE_BLACK:
                        if ((t < 0) || (t > 1)) {
                            continue;
                        }
                        break;
                    case TEXTURE_CLAMP:
                        if (t < 0)
                            t = 0;
                        if (t > 1)
                            t = 1;
                        break;
                }
                // lookup (s,t) and add it to the result
                vector C;
                layer->lookup(C, s, t, context);

                result[0] += C[0] * contribution;
                result[1] += C[1] * contribution;
                result[2] += C[2] * contribution;
            }

            const float tmp = 1 / totalContribution;
            mulvf(result, tmp);
        }

        // textureinfo support
        void getResolution(float *r) {
            r[0] = (float)layer->width;
            r[1] = (float)layer->height;
        }
        int getNumChannels() { return layer->numSamples; }

        CTextureLayer *layer; // There's only one layer
};

///////////////////////////////////////////////////////////////////////
// Class				:	CShadow
// Description			:	A single sided shadow map
// Comments				:
class CShadow : public CEnvironment {
    public:
        CShadow(const char *name, float *toNDC, float *toCamera, CTexture *side) : CEnvironment(name) {
            movmm(this->toNDC, toNDC);
            movmm(this->toCamera, toCamera);
            this->side = side;
        }

        virtual ~CShadow() {
            if (side != NULL)
                delete side;
        }

        void lookup(float *result, const float *D0, const float *D1, const float *D2, const float *D3, CShadingContext *context) {
            float totalContribution = 0;
            const CShadingScratch *scratch = &(context->currentShadingState->scratch);
            const float jitter = 1.0f - 1.0f / (float)scratch->traceParams.samples;

            result[0] = 0;
            for (int i = (int)scratch->traceParams.samples; i > 0; --i) {
                float tmp[4], cP[4];
                float r[4];

                context->random4d.get(r);

                // stratify the sample so that with low sample counts we don't jitter too much
                const float x = (r[0] - 0.5f) * jitter + 0.5f;
                const float y = (r[1] - 0.5f) * jitter + 0.5f;
                const float contribution = scratch->textureParams.filter(x - 0.5f, y - 0.5f, 1, 1);

                totalContribution += contribution;

                cP[COMP_X] = (D0[COMP_X] * (1 - x) + D1[COMP_X] * x) * (1 - y) + (D2[COMP_X] * (1 - x) + D3[COMP_X] * x) * y;
                cP[COMP_Y] = (D0[COMP_Y] * (1 - x) + D1[COMP_Y] * x) * (1 - y) + (D2[COMP_Y] * (1 - x) + D3[COMP_Y] * x) * y;
                cP[COMP_Z] = (D0[COMP_Z] * (1 - x) + D1[COMP_Z] * x) * (1 - y) + (D2[COMP_Z] * (1 - x) + D3[COMP_Z] * x) * y;
                cP[3] = 1;

                mulmp4(tmp, toNDC, cP);
                float s = tmp[0] / tmp[3];
                float t = tmp[1] / tmp[3];

                if (scratch->textureParams.blur > 0) {
                    s += scratch->textureParams.blur * (r[2] - 0.5f);
                    t += scratch->textureParams.blur * (r[3] - 0.5f);
                }

                if ((s < 0) || (s > 1) || (t < 0) || (t > 1)) {
                    continue;
                }

                result[0] += contribution * side->lookupz(s, t, tmp[2] - scratch->traceParams.bias, context);
            }

            result[0] /= totalContribution;
            result[1] = result[0];
            result[2] = result[0];
        }

        // textureinfo support
        void getResolution(float *r) { side->getResolution(r); }
        const char *getTextureType() { return "shadow"; }
        int getNumChannels() { return side->getNumChannels(); }
        int getViewMatrix(float *m) {
            movmm(m, toCamera);
            return TRUE;
        }
        int getProjectionMatrix(float *m) {
            movmm(m, toNDC);
            return TRUE;
        }

    private:
        CTexture *side;
        matrix toNDC;
        matrix toCamera;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDeepShadow
// Description			:	A deep shadow map
// Comments				:
class CDeepShadow : public CEnvironment {

        // Holds a deep tile
        class CDeepTile {
            public:
                float **data;
                float **lastData;
                CTextureBlock block;
        };

    public:
        CDeepShadow(const char *name, const char *fn, const float *toWorld, FILE *in) : CEnvironment(name) {
            matrix mtmp;
            int *tileSizes;

            fileName = strdup(fn);

            // Read the header (portable I/O - Phase 2)
            // Read CDeepShadowHeader fields individually instead of struct
            int32_p xres_i, yres_i, xTiles_i, yTiles_i, tileSize_i, tileShift_i;
            if (!readInt32(in, xres_i) || !readInt32(in, yres_i) ||
                !readInt32(in, xTiles_i) || !readInt32(in, yTiles_i) ||
                !readInt32(in, tileSize_i) || !readInt32(in, tileShift_i)) {
                error(CODE_SYSTEM, "Failed to read deep shadow header integers\n");
                return;
            }
            header.xres = xres_i;
            header.yres = yres_i;
            header.xTiles = xTiles_i;
            header.yTiles = yTiles_i;
            header.tileSize = tileSize_i;
            header.tileShift = tileShift_i;

            // Read transformation matrices
            if (!readFloat32Array(in, header.toNDC, 16) ||
                !readFloat32Array(in, header.toCamera, 16)) {
                error(CODE_SYSTEM, "Failed to read deep shadow header matrices\n");
                return;
            }

            mulmm(mtmp, header.toNDC, toWorld);
            movmm(header.toNDC, mtmp);

            mulmm(mtmp, header.toCamera, toWorld);
            movmm(header.toCamera, mtmp);

            // Read the tile end indices (portable I/O - Phase 2)
            tileIndices = new int[header.xTiles * header.yTiles];
            if (!readInt32Array(in, tileIndices, header.xTiles * header.yTiles)) {
                error(CODE_SYSTEM, "Failed to read deep shadow tile indices\n");
                delete[] tileIndices;
                return;
            }

            // new-style tsm file (portable I/O - Phase 2)
            tileSizes = new int[header.xTiles * header.yTiles];
            if (!readInt32Array(in, tileSizes, header.xTiles * header.yTiles)) {
                error(CODE_SYSTEM, "Failed to read deep shadow tile sizes\n");
                delete[] tileSizes;
                delete[] tileIndices;
                return;
            }

            // Save the index start
            fileStart = ftell(in);

            // Init the tiles
            tiles = new CDeepTile *[header.yTiles];
            for (int k = 0, i = 0; i < header.yTiles; ++i) {

                tiles[i] = new CDeepTile[header.xTiles];

                for (int j = 0; j < header.xTiles; ++j, ++k) {
                    CDeepTile *cTile = tiles[i] + j;
                    int size;

                    size = tileSizes[k];

                    textureRegisterBlock(&(cTile->block), size);
                    cTile->data = new float *[header.tileSize * header.tileSize];
                    cTile->lastData = new float *[header.tileSize * header.tileSize];
                }
            }

            delete[] tileSizes;

            fclose(in);
        }

        virtual ~CDeepShadow() {
            for (int j = 0; j < header.yTiles; ++j) {
                for (int i = 0; i < header.xTiles; ++i) {
                    textureUnregisterBlock(&(tiles[j][i].block));
                    delete[] tiles[j][i].lastData;
                    delete[] tiles[j][i].data;
                }
                delete[] tiles[j];
            }
            delete[] tiles;

            delete[] tileIndices;

            free(fileName);
        }

        void lookup(float *result, const float *D0, const float *D1, const float *D2, const float *D3, CShadingContext *context) {
            float totalContribution = 0;
            const CShadingScratch *scratch = &(context->currentShadingState->scratch);
            const float jitter = 1.0f - 1.0f / (float)scratch->traceParams.samples;

            result[0] = 0;
            result[1] = 0;
            result[2] = 0;
            for (int i = (int)scratch->traceParams.samples; i > 0; --i) {
                float s, t, w;
                float tmp[4], cP[4];
                int px, py;
                int bx, by;
                CDeepTile *cTile;
                float *cPixel;
                float r[4];

                context->random4d.get(r);

                // stratify the sample so that with low sample counts we don't jitter too much
                const float x = (r[0] - 0.5f) * jitter + 0.5f;
                const float y = (r[1] - 0.5f) * jitter + 0.5f;
                const float contribution = scratch->textureParams.filter(x - 0.5f, y - 0.5f, 1, 1);

                totalContribution += contribution;

                cP[COMP_X] = (D0[COMP_X] * (1 - x) + D1[COMP_X] * x) * (1 - y) + (D2[COMP_X] * (1 - x) + D3[COMP_X] * x) * y;
                cP[COMP_Y] = (D0[COMP_Y] * (1 - x) + D1[COMP_Y] * x) * (1 - y) + (D2[COMP_Y] * (1 - x) + D3[COMP_Y] * x) * y;
                cP[COMP_Z] = (D0[COMP_Z] * (1 - x) + D1[COMP_Z] * x) * (1 - y) + (D2[COMP_Z] * (1 - x) + D3[COMP_Z] * x) * y;
                cP[3] = 1;

                mulmp4(tmp, header.toNDC, cP);
                s = tmp[0] / tmp[3];
                t = tmp[1] / tmp[3];

                if (scratch->textureParams.blur > 0) {
                    s += scratch->textureParams.blur * (r[2] - 0.5f);
                    t += scratch->textureParams.blur * (r[3] - 0.5f);
                }

                if ((s < 0) || (s >= 1) || (t < 0) || (t >= 1)) {
                    continue;
                }

                s *= header.xres;
                t *= header.yres;
                w = tmp[2] - scratch->traceParams.bias;

                px = (int)floor(s);
                py = (int)floor(t);
                bx = px >> header.tileShift;
                by = py >> header.tileShift;
                px = px & ((1 << header.tileShift) - 1);
                py = py & ((1 << header.tileShift) - 1);

                cTile = tiles[by] + bx;

                const int thread = context->thread;

                (*CRenderer::textureRefNumber[thread])++;
                cTile->block.threadData[thread].lastRefNumber = *CRenderer::textureRefNumber[thread];

                if (cTile->block.threadData[thread].data == NULL) {
                    loadTile(bx, by, context);
                }

                cPixel = cTile->lastData[py * header.tileSize + px];

                while (TRUE) {
                    if (cPixel[0] > w)
                        cPixel -= 4;
                    else if (cPixel[4] < w)
                        cPixel += 4;
                    else {
                        const float alpha = (w - cPixel[0]) / (cPixel[4] - cPixel[0]);

                        result[0] += (1 - ((1 - alpha) * cPixel[1] + alpha * cPixel[5])) * contribution;
                        result[1] += (1 - ((1 - alpha) * cPixel[2] + alpha * cPixel[6])) * contribution;
                        result[2] += (1 - ((1 - alpha) * cPixel[3] + alpha * cPixel[7])) * contribution;

                        cTile->lastData[py * header.tileSize + px] = cPixel;

                        break;
                    }
                }

                //	0	-	z
                //	1	-	r
                //	2	-	g
                //	3	-	b

                //	4	-	z
                //	5	-	r
                //	6	-	g
                //	7	-	b
            }

            result[0] /= totalContribution;
            result[1] /= totalContribution;
            result[2] /= totalContribution;
        }

        // textureinfo support
        void getResolution(float *r) {
            r[0] = (float)header.xres;
            r[1] = (float)header.yres;
        }
        const char *getTextureType() { return "shadow"; }
        int getNumChannels() { return 4; }
        int getViewMatrix(float *m) {
            movmm(m, header.toCamera);
            return TRUE;
        }
        int getProjectionMatrix(float *m) {
            movmm(m, header.toNDC);
            return TRUE;
        }

    private:
        void loadTile(int x, int y, CShadingContext *context) {

            CDeepTile *cTile = tiles[y] + x;

#ifndef TEXTURE_PERBLOCK_LOCK
            osLock(CRenderer::textureMutex);
#else
            osLock(cTile->block.mutex);
#endif
            if (cTile->block.data != NULL) {
                cTile->block.threadData[context->thread].data = cTile->block.data;
                cTile->block.refCount++;
#ifndef TEXTURE_PERBLOCK_LOCK
                osUnlock(CRenderer::textureMutex);
#else
                osUnlock(cTile->block.mutex);
#endif
                return;
            }

            int index = y * header.xTiles + x;
            FILE *in = fopen(fileName, "rb");
            float **cData;
            float **cLastData;
            float *data, *dataStart;
            int startIndex;

            assert(in != NULL);

            startIndex = tileIndices[index];

            dataStart = data = (float *)textureAllocateBlock(&(cTile->block), context);
            fseek(in, startIndex, SEEK_SET);
            if (fread(data, sizeof(unsigned char), cTile->block.size, in) < (size_t)cTile->block.size) {
                // short read — proceed with whatever data was loaded into the tile
            }
            // fclose(in);  // moved later, see below

            cLastData = cTile->lastData;
            cData = cTile->data;
            for (int i = header.tileSize * header.tileSize; i > 0; --i) {
                cData[0] = data;
                cLastData[0] = data;
                cData++;
                cLastData++;

                if (i != 1) {
                    data += 4;
                    while (*data != -C_INFINITY)
                        data += 4;
                }
            }

            // This is a function call, which the compiler thinks could access
            // any of memory.  Place it here, to ensure the compiler does not
            // hoist the store of the data before we finish computing the
            // data starts and ends on archictectures like PPC which are massively
            // out of order wrt memory store completion
            // the osUnlock will take care of the real barrier
            fclose(in);

            assert(cTile->block.refCount == 0);

            cTile->block.refCount = 1;
            cTile->block.data = dataStart;
            cTile->block.threadData[context->thread].data = dataStart;

#ifndef TEXTURE_PERBLOCK_LOCK
            osUnlock(CRenderer::textureMutex);
#else
            osUnlock(cTile->block.mutex);
#endif
        }

        char *fileName;           // The name of the file
        CDeepTile **tiles;        // Array of tiles
        int *tileIndices;         // The tile offset index in the file
        CDeepShadowHeader header; // The header
        int fileStart;            // The offset in the file
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCubicEnvironment
// Description			:	A Cubic environment map
// Comments				:
class CCubicEnvironment : public CEnvironment {
    public:
        CCubicEnvironment(const char *name, CTexture **s) : CEnvironment(name) {
            sides[0] = s[0];
            sides[1] = s[1];
            sides[2] = s[2];
            sides[3] = s[3];
            sides[4] = s[4];
            sides[5] = s[5];
        }

        virtual ~CCubicEnvironment() {
            if (sides[0] != NULL)
                delete sides[0];
            if (sides[1] != NULL)
                delete sides[1];
            if (sides[2] != NULL)
                delete sides[2];
            if (sides[3] != NULL)
                delete sides[3];
            if (sides[4] != NULL)
                delete sides[4];
            if (sides[5] != NULL)
                delete sides[5];
        }

        typedef enum {
            PX = 0,
            NX,
            PY,
            NY,
            PZ,
            NZ,
        } ESide;

        typedef enum {
            XYZ = 0,
            XZY,
            YXZ,
            YZX,
            ZXY,
            ZYX
        } EOrder;

        void lookup(float *result, const float *D0, const float *D1, const float *D2, const float *D3, CShadingContext *context) {
            EOrder order;
            CTexture *side;
            float u, v;
            vector D;
            float totalContribution = 0;
            vector C;
            const CShadingScratch *scratch = &(context->currentShadingState->scratch);

            result[0] = 0;
            result[1] = 0;
            result[2] = 0;

            if (dotvv(D0, D0) == 0)
                return;

            // Blur the directions if needed
            vector tmp[4];
            if (scratch->textureParams.blur > 0) {
                vector middle;

                // First, compute the mid point
                addvv(middle, D0, D1);
                addvv(middle, D2);
                addvv(middle, D3);
                mulvf(middle, 1.0f / 4.0f);

                // Scale the directions along the mid point with the blur
                subvv(tmp[0], D0, middle);
                normalizevf(tmp[0]);
                mulvf(tmp[0], scratch->textureParams.blur * lengthv(D0) * 2);
                addvv(tmp[0], D0);
                D0 = tmp[0];

                subvv(tmp[1], D1, middle);
                normalizevf(tmp[1]);
                mulvf(tmp[1], scratch->textureParams.blur * lengthv(D1) * 2);
                addvv(tmp[1], D1);
                D1 = tmp[1];

                subvv(tmp[2], D2, middle);
                normalizevf(tmp[2]);
                mulvf(tmp[2], scratch->textureParams.blur * lengthv(D2) * 2);
                addvv(tmp[2], D2);
                D2 = tmp[2];

                subvv(tmp[3], D3, middle);
                normalizevf(tmp[3]);
                mulvf(tmp[3], scratch->textureParams.blur * lengthv(D3) * 2);
                addvv(tmp[3], D3);
                D3 = tmp[3];
            }

            for (int i = (int)scratch->traceParams.samples; i > 0; --i) {
                float t;
                float r[2];

                context->random2d.get(r);

                // stratify the sample so that with low sample counts we don't jitter too much
                const float x = (r[0] - 0.5f) + 0.5f;
                const float y = (r[1] - 0.5f) + 0.5f;
                const float contribution = scratch->textureParams.filter(x - 0.5f, y - 0.5f, 1, 1);

                totalContribution += contribution;

                D[0] = (D0[0] * (1 - x) + D1[0] * x) * (1 - y) + (D2[0] * (1 - x) + D3[0] * x) * y;
                D[1] = (D0[1] * (1 - x) + D1[1] * x) * (1 - y) + (D2[1] * (1 - x) + D3[1] * x) * y;
                D[2] = (D0[2] * (1 - x) + D1[2] * x) * (1 - y) + (D2[2] * (1 - x) + D3[2] * x) * y;

                // Find the side of the cube that we're looking at
                if (fabs(D[COMP_Y]) > fabs(D[COMP_X])) {
                    if (fabs(D[COMP_Z]) > fabs(D[COMP_Y])) {
                        order = ZYX;
                    }
                    else {
                        if (fabs(D[COMP_Z]) > fabs(D[COMP_X]))
                            order = YZX;
                        else
                            order = YXZ;
                    }
                }
                else if (fabs(D[COMP_Z]) > fabs(D[COMP_Y])) {
                    if (fabs(D[COMP_Z]) > fabs(D[COMP_X]))
                        order = ZXY;
                    else
                        order = XZY;
                }
                else {
                    order = XYZ;
                }

                switch (order) {
                    case XYZ:
                    case XZY:
                        if (D[COMP_X] > 0) {
                            side = sides[PX];
                            t = 1 / D[COMP_X];
                            u = (-D[COMP_Z] * t + 1) * 0.5f;
                            v = (-D[COMP_Y] * t + 1) * 0.5f;
                        }
                        else {
                            side = sides[NX];
                            t = -1 / D[COMP_X];
                            u = (D[COMP_Z] * t + 1) * 0.5f;
                            v = (-D[COMP_Y] * t + 1) * 0.5f;
                        }
                        break;
                    case YXZ:
                    case YZX:
                        if (D[COMP_Y] > 0) {
                            side = sides[PY];
                            t = 1 / D[COMP_Y];
                            u = (D[COMP_X] * t + 1) * 0.5f;
                            v = (D[COMP_Z] * t + 1) * 0.5f;
                        }
                        else {
                            side = sides[NY];
                            t = -1 / D[COMP_Y];
                            u = (D[COMP_X] * t + 1) * 0.5f;
                            v = (-D[COMP_Z] * t + 1) * 0.5f;
                        }
                        break;
                    case ZXY:
                    case ZYX:
                        if (D[COMP_Z] > 0) {
                            side = sides[PZ];
                            t = 1 / D[COMP_Z];
                            u = (D[COMP_X] * t + 1) * 0.5f;
                            v = (-D[COMP_Y] * t + 1) * 0.5f;
                        }
                        else {
                            side = sides[NZ];
                            t = -1 / D[COMP_Z];
                            u = (-D[COMP_X] * t + 1) * 0.5f;
                            v = (-D[COMP_Y] * t + 1) * 0.5f;
                        }

                        break;
                }

                side->lookup(C, u, v, context);

                result[0] += C[0] * contribution;
                result[1] += C[1] * contribution;
                result[2] += C[2] * contribution;
            }

            mulvf(result, 1 / totalContribution);
        }

        // textureinfo support
        void getResolution(float *r) { sides[0]->getResolution(r); }
        const char *getTextureType() { return "environment"; }
        int getNumChannels() { return sides[0]->getNumChannels(); }
        int getViewMatrix(float *) { return FALSE; }
        int getProjectionMatrix(float *) { return FALSE; }

        CTexture *sides[6];
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSphericalEnvironment
// Description			:	A Spherical environment map
// Comments				:	RenderMan does not support cylinderical env. maps yet so this class is not used
class CSphericalEnvironment : public CEnvironment {
    public:
        CSphericalEnvironment(const char *name, CTexture *s) : CEnvironment(name) {
            side = s;
        }

        virtual ~CSphericalEnvironment() {
            delete side;
        }

        void lookup(float *result, const float *D0, const float *D1, const float *D2, const float *D3, CShadingContext *context) {
            float u[4], v[4];
            float m;

            m = 2 * sqrtf(D0[COMP_X] * D0[COMP_X] + D0[COMP_Y] * D0[COMP_Y] + (D0[COMP_Z] + 1) * (D0[COMP_Z] + 1));
            u[0] = D0[COMP_X] / m + 0.5f;
            v[0] = D0[COMP_Y] / m + 0.5f;

            m = 2 * sqrtf(D1[COMP_X] * D1[COMP_X] + D1[COMP_Y] * D1[COMP_Y] + (D1[COMP_Z] + 1) * (D1[COMP_Z] + 1));
            u[1] = D1[COMP_X] / m + 0.5f;
            v[1] = D1[COMP_Y] / m + 0.5f;

            m = 2 * sqrtf(D2[COMP_X] * D2[COMP_X] + D2[COMP_Y] * D2[COMP_Y] + (D2[COMP_Z] + 1) * (D2[COMP_Z] + 1));
            u[2] = D2[COMP_X] / m + 0.5f;
            v[2] = D2[COMP_Y] / m + 0.5f;

            m = 2 * sqrtf(D3[COMP_X] * D3[COMP_X] + D3[COMP_Y] * D3[COMP_Y] + (D3[COMP_Z] + 1) * (D3[COMP_Z] + 1));
            u[3] = D3[COMP_X] / m + 0.5f;
            v[3] = D3[COMP_Y] / m + 0.5f;

            side->lookup4(result, u, v, context);
        }

        // textureinfo support
        void getResolution(float *r) { side->getResolution(r); }
        const char *getTextureType() { return "environment"; }
        int getNumChannels() { return side->getNumChannels(); }
        int getViewMatrix(float *) { return FALSE; }
        int getProjectionMatrix(float *) { return FALSE; }

        CTexture *side;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCylindericalEnvironment
// Description			:	A Cylinderical environment map
// Comments				:
class CCylindericalEnvironment : public CEnvironment {
    public:
        CCylindericalEnvironment(const char *name, CTexture *s) : CEnvironment(name) {
            side = s;
        }

        virtual ~CCylindericalEnvironment() {
            delete side;
        }

        void lookup(float *result, const float *D0, const float *D1, const float *D2, const float *D3, CShadingContext *context) {
            float u[4], v[4];
            double a, b, c;
            vector D, Dsamp;

            // Note Cylinderical maps (EnvLatL maps) are Z is north
            // -pi is top +pi is bottom, hence sign flip in z

            if (dotvv(D0, D0) > 0) {
                normalizev(D, D0);
                movvv(Dsamp, D);
                u[0] = (atan2f(D[COMP_Y], D[COMP_X]) + (float)C_PI) * (1.0f / (2.0f * (float)C_PI));
                v[0] = asin(-D[COMP_Z]) * (1.0f / (float)C_PI) + 0.5f;

                c = (D[COMP_X] * D[COMP_X] + D[COMP_Y] * D[COMP_Y]) * 2.0 * C_PI;
                a = -D[COMP_Y] / c;
                b = D[COMP_X] / c;
                c = 1.0f / (C_PI * sqrt(1.0f - D[COMP_Z] * D[COMP_Z] + C_EPSILON)); // would be -ve but cancels with -y

                normalizev(D, D1);
                subvv(D, Dsamp);
                u[1] = u[0] + (float)(a * D[COMP_X] + b * D[COMP_Y]);
                v[1] = v[0] + (float)(c * D[COMP_Z]);

                normalizev(D, D2);
                subvv(D, Dsamp);
                u[2] = u[0] + (float)(a * D[COMP_X] + b * D[COMP_Y]);
                v[2] = v[0] + (float)(c * D[COMP_Z]);

                normalizev(D, D3);
                subvv(D, Dsamp);
                u[3] = u[0] + (float)(a * D[COMP_X] + b * D[COMP_Y]);
                v[3] = v[0] + (float)(c * D[COMP_Z]);

                side->lookup4(result, u, v, context);
            }
            else {
                initv(result, 0, 0, 0);
            }
        }

        // textureinfo support
        void getResolution(float *r) { side->getResolution(r); }
        const char *getTextureType() { return "environment"; }
        int getNumChannels() { return side->getNumChannels(); }
        int getViewMatrix(float *) { return FALSE; }
        int getProjectionMatrix(float *) { return FALSE; }

        CTexture *side;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyTexture
// Method				:	lookupz
// Description			:	Dummy
// Return Value			:
// Comments				:
float CDummyTexture::lookupz(float, float, float, CShadingContext *) {
    return 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyTexture
// Method				:	lookup
// Description			:	Dummy
// Return Value			:
// Comments				:
void CDummyTexture::lookup(float *dest, float, float, CShadingContext *context) {
    initv(dest, context->currentShadingState->scratch.textureParams.fill);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyTexture
// Method				:	lookup4
// Description			:	Dummy
// Return Value			:
// Comments				:
void CDummyTexture::lookup4(float *dest, const float *, const float *, CShadingContext *context) {
    initv(dest, context->currentShadingState->scratch.textureParams.fill);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyEnvironment
// Method				:	lookup
// Description			:	Dummy
// Return Value			:
// Comments				:
void CDummyEnvironment::lookup(float *dest, const float *, const float *, const float *, const float *, CShadingContext *context) {
    initv(dest, context->currentShadingState->scratch.textureParams.fill);
}

///////////////////////////////////////////////////////////////////////
// Function				:	readTexture
// Description			:	Read the pyramid layers
// Return Value			:	TRUE on success
// Comments				:
template <class T>
static CTexture *readMadeTexture(const char *name, const char *aname, TIFF *in, int &dstart, int width, int height, const char *smode, const char *tmode, T) {
    uint32_t fileWidth, fileHeight;
    uint32_t tileWidth, tileHeight;
    uint16_t numSamples;
    TTextureMode sMode, tMode;
    int tileWidthShift, tileHeightShift;
    double M;

    fileWidth = 0;
    fileHeight = 0;
    numSamples = 0;

    TIFFSetDirectory(in, dstart);
    // Note: using imagewidth rather than pixar full width is OK
    // we want to work out how many pyramid levels there are
    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEWIDTH, &fileWidth);
    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGELENGTH, &fileHeight);
    TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &numSamples);

    if (strcmp(smode, RI_PERIODIC) == 0) {
        sMode = TEXTURE_PERIODIC;
    }
    else if (strcmp(smode, RI_CLAMP) == 0) {
        sMode = TEXTURE_CLAMP;
    }
    else if ((strcmp(smode, RI_BLACK) == 0) || (strcmp(smode, RI_NONPERIODIC) == 0)) {
        sMode = TEXTURE_BLACK;
    }
    else {
        error(CODE_BADTOKEN, "Unknown texture wrap mode (\"%s\")\n", smode);
        sMode = TEXTURE_BLACK;
    }

    if (strcmp(tmode, RI_PERIODIC) == 0) {
        tMode = TEXTURE_PERIODIC;
    }
    else if (strcmp(tmode, RI_CLAMP) == 0) {
        tMode = TEXTURE_CLAMP;
    }
    else if ((strcmp(smode, RI_BLACK) == 0) || (strcmp(smode, RI_NONPERIODIC) == 0)) {
        tMode = TEXTURE_BLACK;
    }
    else {
        error(CODE_BADTOKEN, "Unknown texture wrap mode (\"%s\")\n", tmode);
        tMode = TEXTURE_BLACK;
    }

    CMadeTexture *cTexture = new CMadeTexture(aname);

    if (sizeof(T) == sizeof(float)) {
        M = 1;
    }
    else if (sizeof(T) == sizeof(unsigned short)) {
        // This doesn't really make sense to me, but it does give correct results.
        // It seems that pixar-txmake generated textures have the photometric RGB flag
        // and they want to be dealt with by dividing the 16-bit val by 32k, not 65k.
        // I guess that means it's a signed 16-bit value in pixar-land ?
        unsigned short photoMetric = 0;
        TIFFGetFieldDefaulted(in, TIFFTAG_PHOTOMETRIC, &photoMetric);
        if (photoMetric == PHOTOMETRIC_RGB)
            M = 1.0 / 65535.0 * 2.0;
        else
            M = 1.0 / 65535.0;
    }
    else {
        M = 1.0 / 255.0;
    }

    // This is the number of levels we have for the texture
    const int pyramidSize = tiffNumLevels(fileWidth, fileHeight);

    cTexture->numLayers = pyramidSize;
    cTexture->layers = new CTextureLayer *[pyramidSize];

    int cwidth = width;
    int cheight = height;
    for (int i = 0; i < pyramidSize; ++i) {
        TIFFSetDirectory(in, dstart);
        TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEWIDTH, &fileWidth);
        TIFFGetFieldDefaulted(in, TIFFTAG_IMAGELENGTH, &fileHeight);

        TIFFGetFieldDefaulted(in, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetFieldDefaulted(in, TIFFTAG_TILELENGTH, &tileHeight);
        int ii, jj;
        for (ii = 1, jj = 0; (uint32_t)ii != tileWidth; ii = ii << 1, jj++)
            ;
        tileWidthShift = jj;
        for (ii = 1, jj = 0; (uint32_t)ii != tileHeight; ii = ii << 1, jj++)
            ;
        tileHeightShift = jj;

        cTexture->layers[i] = new CTiledTexture<T>(new CTiffTileSource(name, dstart), cwidth, cheight, numSamples, fileWidth, fileHeight, sMode, tMode, tileWidth, tileWidthShift, tileHeight, tileHeightShift, M);
        dstart++;

        cwidth = cwidth >> 1;
        cheight = cheight >> 1;
    }

    return cTexture;
}

///////////////////////////////////////////////////////////////////////
// Function				:	readSynthesizedTexture
// Description			:	Builds a CMadeTexture from an already-built
//							CSynthesizedPyramid, one CTiledTexture<T> layer
//							per pyramid level -- the in-memory counterpart
//							to readMadeTexture() above, sourcing per-level
//							geometry from the pyramid instead of TIFF tags
//							(spec 020, 020-runtime-tile-paging, T009).
// Return Value			:	The texture
// Comments				:	Periodic wrap mode both axes unconditionally
//							(FR-013's resolved default) -- there is no RIB
//							parameter list to read a wrap mode override
//							from at texture-lookup time, unlike a baked
//							texture's TIFFTAG_PIXAR_WRAPMODES tag.
template <class T>
static CTexture *readSynthesizedTexture(const char *aname, std::shared_ptr<CSynthesizedPyramid> pyramid) {
    double M;

    if (sizeof(T) == sizeof(float)) {
        M = 1;
    }
    else if (sizeof(T) == sizeof(unsigned short)) {
        // No PHOTOMETRIC_RGB half-range (32k, not 65k) heuristic here --
        // that heuristic (see readMadeTexture() above / appendLayer()'s
        // own comment, texmake.cpp) is for pixar-txmake-style legacy
        // files specifically; a synthesized pyramid's 16-bit samples use
        // the full 0-65535 range, matching what this codebase's own
        // appendLayer() writes for a fresh bake.
        M = 1.0 / 65535.0;
    }
    else {
        M = 1.0 / 255.0;
    }

    int ii, jj;
    for (ii = 1, jj = 0; ii != DEFAULT_TILE_SIZE; ii = ii << 1, jj++)
        ;
    const int tileSizeShift = jj;

    CMadeTexture *cTexture = new CMadeTexture(aname);

    const int numLevels = (int)pyramid->levels.size();
    cTexture->numLayers = (short)numLevels;
    cTexture->layers = new CTextureLayer *[numLevels];

    int cwidth = pyramid->validWidth;
    int cheight = pyramid->validHeight;

    for (int i = 0; i < numLevels; i++) {
        const CSynthesizedPyramid::Level &lvl = pyramid->levels[i];

        cTexture->layers[i] = new CTiledTexture<T>(new CSynthesizedTileSource(pyramid, i), cwidth, cheight, (short)pyramid->numChannels, lvl.width, lvl.height, TEXTURE_PERIODIC, TEXTURE_PERIODIC, DEFAULT_TILE_SIZE, tileSizeShift, DEFAULT_TILE_SIZE, tileSizeShift, M);

        cwidth = cwidth >> 1;
        cheight = cheight >> 1;
    }

    return cTexture;
}

///////////////////////////////////////////////////////////////////////
// Function				:	readTexture
// Description			:	read a regular texture
// Return Value			:	The texture
// Comments				:
template <class T>
static CTexture *readTexture(const char *name, const char *aname, TIFF *in, int &dstart, T) {
    TIFFSetDirectory(in, dstart);
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t numSamples = 0;
    // Note: regular textures don't have pixar full image tag
    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &numSamples);

    double M;
    if (sizeof(T) == sizeof(float)) {
        M = 1;
    }
    else if (sizeof(T) == sizeof(unsigned short)) {
        M = 1.0 / 65535.0;
    }
    else {
        M = 1.0 / 255.0;
    }

    CRegularTexture *cTexture = new CRegularTexture(aname);
    cTexture->layer = new CBasicTexture<T>(new CTiffTileSource(name, dstart), width, height, numSamples, width, height, TEXTURE_BLACK, TEXTURE_BLACK, M);
    dstart++;

    return cTexture;
}

///////////////////////////////////////////////////////////////////////
// Function				:	textureLoad
// Description			:	Load a texture from disk
// Return Value			:	Pointer to the new texture
// Comments				:
static CTexture *texLoad(const char *name, const char *aname, TIFF *in, int &dstart, int unMade = FALSE) {
    CTexture *cTexture = NULL;
    uint16_t bitspersample;

    // Get the bits per sample from the file
    TIFFSetDirectory(in, dstart);
    TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);

    // Is this a made texture file ?
    if (unMade == FALSE) {
        uint32_t width = 0;
        uint32_t height = 0;

        // Use full image tags in preference to image size tags
        // Allows us to rescale when otexmake makes a texture power of 2
        // but retains aspect ratio
        if (((TIFFGetField(in, TIFFTAG_PIXAR_IMAGEFULLWIDTH, &width) == 1) &&
             (TIFFGetField(in, TIFFTAG_PIXAR_IMAGEFULLLENGTH, &height) == 1)) ||
            ((TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width) == 1) &&
             (TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height) == 1))) {

            // The input must be tiled
            if (TIFFIsTiled(in)) {
                char *mode = NULL;

                // The input must contain wrap modes
                if (TIFFGetField(in, TIFFTAG_PIXAR_WRAPMODES, &mode) == 1) {
                    char *smode, *tmode, tmp[128];

                    strcpy(tmp, mode);
                    smode = tmp;
                    tmode = strchr(smode, ',');

                    if (tmode != NULL) {
                        *tmode++ = '\0';

                        if (bitspersample == 8) {
                            cTexture = readMadeTexture<unsigned char>(name, aname, in, dstart, width, height, smode, tmode, 1);
                        }
                        else if (bitspersample == 16) {
                            cTexture = readMadeTexture<unsigned short>(name, aname, in, dstart, width, height, smode, tmode, 1);
                        }
                        else {
                            cTexture = readMadeTexture<float>(name, aname, in, dstart, width, height, smode, tmode, 1);
                        }
                    }
                }
                else {
                    if (bitspersample == 8) {
                        cTexture = readMadeTexture<unsigned char>(name, aname, in, dstart, width, height, RI_BLACK, RI_BLACK, 1);
                    }
                    else if (bitspersample == 16) {
                        cTexture = readMadeTexture<unsigned short>(name, aname, in, dstart, width, height, RI_BLACK, RI_BLACK, 1);
                    }
                    else {
                        cTexture = readMadeTexture<float>(name, aname, in, dstart, width, height, RI_BLACK, RI_BLACK, 1);
                    }
                }
            }
        }
    }

    // Were we able to read the texture ?
    if (cTexture == NULL) {

        // This must be an un-made texture then
        if (bitspersample == 8) {
            cTexture = readTexture<unsigned char>(name, aname, in, dstart, 1);
        }
        else if (bitspersample == 16) {
            cTexture = readTexture<unsigned short>(name, aname, in, dstart, 1);
        }
        else {
            cTexture = readTexture<float>(name, aname, in, dstart, 1);
        }
    }

    // We must have a texture
    assert(cTexture != NULL);

    return cTexture;
}

///////////////////////////////////////////////////////////////////////
// Function				:	looksLikeTiff
// Description			:	Cheaply checks fn's first 4 bytes against
//							TIFF's own magic number (both byte orders),
//							without ever calling TIFFOpen() (spec 020,
//							020-runtime-tile-paging, T009 scope
//							correction -- see that task's own note and
//							research.md SS4b for why this exists).
// Return Value			:	true if fn's magic number matches TIFF (or
//							the check itself couldn't be performed, in
//							which case the caller should still attempt
//							TIFFOpen() rather than silently divert a file
//							this check could not read); false only for a
//							successfully-read header that is definitively
//							NOT a TIFF.
// Comments				:	A valid-magic-but-otherwise-corrupt TIFF
//							still reaches TIFFOpen() and still reports a
//							real error -- this only short-circuits the
//							"not a TIFF at all" case.
static bool looksLikeTiff(const char *fn) {
    FILE *probe = fopen(fn, "rb");
    if (probe == NULL) {
        return true; // Should-never-happen (locateFile already found fn) -- default to attempting TIFFOpen.
    }

    unsigned char magic[4];
    size_t numRead = fread(magic, 1, sizeof(magic), probe);
    fclose(probe);

    if (numRead != sizeof(magic)) {
        return true; // Couldn't read a full header -- let TIFFOpen() decide.
    }

    const bool little = (magic[0] == 0x49) && (magic[1] == 0x49) && (magic[2] == 0x2A) && (magic[3] == 0x00);
    const bool big = (magic[0] == 0x4D) && (magic[1] == 0x4D) && (magic[2] == 0x00) && (magic[3] == 0x2A);

    return little || big;
}

///////////////////////////////////////////////////////////////////////
// Function				:	resolveDefaultTextureCacheDirectory
// Description			:	Resolves the default disk-cache directory used
//							when `Option "texturecache" "directory"` is not
//							given (spec 020, 020-runtime-tile-paging, T017).
// Return Value			:	A directory path, always ending in '/'.
// Comments				:	Deliberately NOT osTempdir() -- that helper is
//							documented as "unique within this process"
//							(os.cpp, pid-suffixed), which would mean two
//							separate orender invocations against the same
//							source could never see each other's cache
//							entries, defeating this entire feature (User
//							Story 2's own independent test: "render the
//							same unbaked-source scene twice... confirm the
//							second render reuses the cached entry"). This
//							uses the same TMPDIR/TMP-then-/tmp resolution
//							osTempdir() itself uses, but under one fixed,
//							stable subdirectory shared by every render on
//							this machine.
static std::string resolveDefaultTextureCacheDirectory() {
    const char *tempDirEnv = osEnvironment("TMPDIR");
    if (tempDirEnv == NULL)
        tempDirEnv = osEnvironment("TMP");

    std::string directory = (tempDirEnv != NULL) ? tempDirEnv : "/tmp";
    if (!directory.empty() && (directory.back() != '/'))
        directory += '/';
    directory += "openRenderTextureCache/";

    return directory;
}

///////////////////////////////////////////////////////////////////////
// Function				:	computeTextureCacheFilePath
// Description			:	Computes the disk-cache key/path for sourcePath
//							(spec 020, 020-runtime-tile-paging, T017):
//							<hash-of-absolute-source-path>-<source-mtime-
//							epoch-seconds>.tex, under the configured (or
//							default, see resolveDefaultTextureCacheDirectory
//							above) cache directory (research.md SS5). "Is
//							there a valid, fresh cache entry" reduces to "is
//							there a file at this exact path" -- a changed
//							source resolves to a different filename
//							entirely, so no separate staleness check is
//							needed (FR-006).
// Return Value			:	true on success (cachePath filled); false if
//							sourcePath's absolute path or mtime could not be
//							determined (should-never-happen -- the caller
//							already decoded sourcePath successfully via
//							CImageInput).
// Comments				:	Creates the cache directory (best-effort,
//							ignoring failure -- an unwritable location
//							degrades to in-memory-only per FR-007, detected
//							later when the actual write attempt fails).
static bool computeTextureCacheFilePath(const char *sourcePath, std::string &cachePath) {
    char absolutePath[OS_MAX_PATH_LENGTH];
    if (realpath(sourcePath, absolutePath) == NULL) {
        return false;
    }

    struct stat st;
    if (stat(absolutePath, &st) != 0) {
        return false;
    }

    std::string directory = (CRenderer::textureCacheDirectory != NULL) ? CRenderer::textureCacheDirectory : resolveDefaultTextureCacheDirectory();
    if (!directory.empty() && (directory.back() != '/'))
        directory += '/';

    osCreateDir(directory.c_str()); // best-effort; mkdir() failure (e.g. EEXIST, or unwritable parent) is not fatal here

    const size_t hash = std::hash<std::string>{}(absolutePath);
    char filename[64];
    snprintf(filename, sizeof(filename), "%zx-%lld.tex", hash, (long long)st.st_mtime);

    cachePath = directory + filename;

    return true;
}

///////////////////////////////////////////////////////////////////////
// Function				:	writeTextureCacheEntry
// Description			:	Best-effort disk-cache write for sourcePath,
//							targeting finalCachePath (spec 020,
//							020-runtime-tile-paging, T018). Reuses the
//							existing makeTexture() (texmake.h) -- already
//							renderer-linked, already dispatches through
//							createImageInput() -- as the writer, so this
//							spec invents no new TIFF-pyramid-writing code
//							(research.md SS8).
// Return Value			:	-
// Comments				:	Writes to a temporary, uniquely-named
//							(PID-suffixed) path in the SAME directory as
//							finalCachePath first, then rename()s it into
//							place only once fully written (research.md
//							SS6) -- a reader only ever attempts
//							finalCachePath, so it can never observe a
//							partial write, even when several render
//							processes race to populate the same entry
//							(FR-016/SC-006). A write failure (unwritable
//							location) simply leaves nothing to rename;
//							this never fails or alters the render (FR-007)
//							-- the caller has already proceeded via the
//							in-memory CSynthesizedTileSource path
//							regardless.
static void writeTextureCacheEntry(const char *sourcePath, const std::string &finalCachePath) {
    char tempPath[OS_MAX_PATH_LENGTH];
    snprintf(tempPath, sizeof(tempPath), "%s.tmp%d", finalCachePath.c_str(), (int)getpid());

    makeTexture(sourcePath, tempPath, CRenderer::texturePath, RI_PERIODIC, RI_PERIODIC, RiCatmullRomFilter, 3.0f, 3.0f, 0, NULL, NULL);

    if (osFileExists(tempPath)) {
        rename(tempPath, finalCachePath.c_str());
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	textureLoad
// Description			:	Load a texture from disk
// Return Value			:	Pointer to the new texture
// Comments				:
CTexture *CRenderer::textureLoad(const char *name, TSearchpath *path) {
    char fn[OS_MAX_PATH_LENGTH];

    if (CRenderer::locateFile(fn, name, path) == FALSE) {
        return NULL;
    }

    // Set the error handler so we don't crash
    TIFFSetErrorHandler(tiffErrorHandler);
    TIFFSetWarningHandler(tiffErrorHandler);

    // Open the texture -- but only attempt TIFFOpen() at all when fn's
    // own magic number says it might actually be one. TIFFSetErrorHandler
    // above is process-global (not per-call, not thread-local) and
    // textureLoad() is reachable from multiple shading threads
    // concurrently (research.md SS4a); a definitely-not-TIFF file (e.g.
    // any PNG/EXR/RGBE source this spec's own fallback below is meant to
    // serve) would otherwise make libtiff invoke tiffErrorHandler() ->
    // error(CODE_SYSTEM, "Not a TIFF...") on every single load, which
    // sets the global RiLastError and makes orender's own exit code
    // nonzero (orender.cpp:919) even though the render itself succeeds
    // end to end via the fallback -- and test_hider_parity.cpp (the
    // harness T011's new parity tests run under) treats any nonzero
    // orender exit code as an outright failure before it ever compares
    // pixels (test_hider_parity.cpp:339-343), which would make every
    // new unbaked-source scene fail regardless of correctness. Found via
    // an actual smoke render during T009/T010, not assumed. A
    // save-current-handler/suppress/restore alternative around just this
    // probe was considered and rejected: unsafe under the same
    // concurrency, since it would suppress a DIFFERENT thread's genuine
    // TIFFOpen error during the suppression window. This magic-number
    // check touches no shared state, so it introduces no new race.
    TIFF *in = looksLikeTiff(fn) ? TIFFOpen(fn, "r") : NULL;
    CTexture *cTexture = NULL;
    if (in != NULL) {
        char *textureFormat = NULL;
        int directory = 0;

        if (TIFFGetField(in, TIFFTAG_PIXAR_TEXTUREFORMAT, &textureFormat) == 1) {
            if (strcmp(textureFormat, TIFF_TEXTURE) == 0)
                // This seems like a made texture
                cTexture = texLoad(fn, name, in, directory);
            else
                // This seems like a texture made by another software
                cTexture = texLoad(fn, name, in, directory);
        }
        else {
            // This seems like an unmade texture
            cTexture = texLoad(fn, name, in, directory);
        }

        TIFFClose(in);
    }
    else {
        // Not a TIFF at all -- try decoding it directly via CImageInput
        // and serving it through the in-memory synthesized-pyramid
        // backend (spec 020, 020-runtime-tile-paging). Reached only when
        // TIFFOpen() has already failed, so the existing baked-TIFF fast
        // path above is completely unaffected (FR-002). Periodic wrap
        // mode both axes (FR-013's resolved default) -- there is no RIB
        // parameter list to read a wrap-mode override from here, unlike
        // a baked texture's TIFFTAG_PIXAR_WRAPMODES tag.
        std::string cachePath;
        bool haveCachePath = false;

        if (CRenderer::textureCacheEnabled) {
            // Opt-in disk cache (T017/T018, contracts/texturecache-option.md):
            // a cache hit reads back through the completely ordinary,
            // unmodified CTiffTileSource fast path above -- no new
            // read-side code at all.
            haveCachePath = computeTextureCacheFilePath(fn, cachePath);
            if (haveCachePath) {
                // Unlike looksLikeTiff()'s use on the SOURCE file above
                // (whose existence locateFile() already confirmed, making
                // an unreadable-file fallback truly should-never-happen),
                // the cache path legitimately may not exist yet -- a
                // first-time miss is the normal, expected case, not an
                // error. Check existence explicitly first, so a miss
                // never reaches TIFFOpen() at all (avoiding the same
                // spurious-error/exit-code hazard looksLikeTiff() itself
                // exists to avoid for the source-file check, T009).
                TIFF *cacheIn = (osFileExists(cachePath.c_str()) && looksLikeTiff(cachePath.c_str())) ? TIFFOpen(cachePath.c_str(), "r") : NULL;
                if (cacheIn != NULL) {
                    int directory = 0;
                    cTexture = texLoad(cachePath.c_str(), name, cacheIn, directory);
                    TIFFClose(cacheIn);
                }
            }
        }

        if (cTexture == NULL) {
            // Cache disabled, cache miss, or no usable cache path --
            // decode+synthesize in memory (T009's original path).
            std::shared_ptr<CSynthesizedPyramid> pyramid = buildSynthesizedPyramid(fn);
            if (pyramid) {
                if (CRenderer::textureCacheEnabled && haveCachePath) {
                    // Best-effort disk-cache write (T018) -- never fails
                    // or alters this render either way (FR-007); the
                    // in-memory path below proceeds regardless.
                    writeTextureCacheEntry(fn, cachePath);
                }

                if (pyramid->bitsPerSample == 8) {
                    cTexture = readSynthesizedTexture<unsigned char>(name, pyramid);
                }
                else if (pyramid->bitsPerSample == 16) {
                    cTexture = readSynthesizedTexture<unsigned short>(name, pyramid);
                }
                else {
                    cTexture = readSynthesizedTexture<float>(name, pyramid);
                }
            }
            // On failure (unrecognized/unsupported format, or a decode
            // failure), cTexture stays NULL -- CRenderer::getTexture()'s
            // existing CDummyTexture substitution + CODE_NOFILE error
            // (rendererFiles.cpp:373) handles it exactly as it always has.
        }
    }

    return cTexture;
}

///////////////////////////////////////////////////////////////////////
// Function				:	environmentLoad
// Description			:	Load an environment from disk
// Return Value			:	Pointer to the new environment
// Comments				:
CEnvironment *CRenderer::environmentLoad(const char *name, TSearchpath *path, float *toWorld) {
    char fileName[OS_MAX_PATH_LENGTH];

    if (CRenderer::locateFile(fileName, name, path) == FALSE) {
        return NULL;
    }

    // Check if the file is a transparency shadow map
    FILE *tmpin = ropen(fileName, "rb", fileTransparencyShadow, TRUE);
    if (tmpin != NULL) {
        return new CDeepShadow(name, fileName, toWorld, tmpin);
    }

    // Set the error handler so we don't crash
    TIFFSetErrorHandler(tiffErrorHandler);
    TIFFSetWarningHandler(tiffErrorHandler);

    // Open the texture
    TIFF *in = TIFFOpen(fileName, "r");
    CEnvironment *cTexture = NULL;
    if (in != NULL) {
        char *textureFormat = NULL;

        if (TIFFGetField(in, TIFFTAG_PIXAR_TEXTUREFORMAT, &textureFormat) == 1) {

            if (strcmp(textureFormat, TIFF_CUBIC_ENVIRONMENT) == 0) {
                // We're loading a cubic environment
                int directory = 0;
                CTexture *sides[6];

                for (int i = 0; i < 6; ++i) {
                    sides[i] = texLoad(fileName, name, in, directory);
                }

                cTexture = new CCubicEnvironment(name, sides);
            }
            else if (strcmp(textureFormat, TIFF_SPHERICAL_ENVIRONMENT) == 0) {
                int directory = 0;
                CTexture *side;

                side = texLoad(fileName, name, in, directory);

                cTexture = new CSphericalEnvironment(name, side);
            }
            else if (strcmp(textureFormat, TIFF_CYLINDER_ENVIRONMENT) == 0) {
                int directory = 0;
                CTexture *side;

                side = texLoad(fileName, name, in, directory);

                cTexture = new CCylindericalEnvironment(name, side);
            }
            else if (strcmp(textureFormat, TIFF_SHADOW) == 0) {
                CTexture *side;
                int directory = 0;
                matrix worldToCamera, worldToScreen, localToNDC, localToCamera;
                float *tmp;

                TIFFGetField(in, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, &tmp);
                movmm(worldToCamera, tmp);
                TIFFGetField(in, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, &tmp);
                movmm(worldToScreen, tmp);

                side = texLoad(fileName, name, in, directory);

                // Compute the transformation matrix to the light space
                mulmm(localToNDC, worldToScreen, toWorld);
                mulmm(localToCamera, worldToCamera, toWorld);

                cTexture = new CShadow(name, localToNDC, localToCamera, side);
            }
        }
    }

    return cTexture;
}

///////////////////////////////////////////////////////////////////////
// Function				:	initTextures
// Description			:	Set the maximum texture memory and init texturing
// Return Value			:	-
// Comments				:
void CRenderer::initTextures(int mm) {
    // Set up our texturing
    const int maxPerThread = (int)ceil((float)mm / CRenderer::numThreads);

    CRenderer::textureUsedBlocks = NULL;

    CRenderer::textureUsedMemory = new int[CRenderer::numThreads];
    CRenderer::textureMaxMemory = new int[CRenderer::numThreads];

    CRenderer::textureRefNumber = new int *[CRenderer::numThreads];

    for (int i = 0; i < CRenderer::numThreads; ++i) {
        CRenderer::textureMaxMemory[i] = maxPerThread;
        CRenderer::textureUsedMemory[i] = 0;

        CRenderer::textureRefNumber[i] = new int;
        *CRenderer::textureRefNumber[i] = 0;
    }

    // Note: all memory should have been cleared by previous shutdown
}

///////////////////////////////////////////////////////////////////////
// Function				:	shutdownTextures
// Description			:	Set the maximum texture memory and init texturing
// Return Value			:	-
// Comments				:
void CRenderer::shutdownTextures() {

    // Flush all the memory (blocks will be killed by earlier file shutdown)

    // for (int i=0;i<CRenderer::numThreads;i++) {
    //	textureMemFlush(NULL,i,TRUE);
    //	assert(CRenderer::textureUsedMemory[i] == 0);
    // }

    assert(CRenderer::textureUsedBlocks == NULL);

    // free up our texturing counters
    delete[] CRenderer::textureUsedMemory;
    delete[] CRenderer::textureMaxMemory;

    // Get rid of the reference counter
    for (int i = 0; i < CRenderer::numThreads; ++i)
        delete CRenderer::textureRefNumber[i];
    delete[] CRenderer::textureRefNumber;
}
