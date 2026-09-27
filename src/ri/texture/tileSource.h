/**
 * Project: openRender
 *
 * File: tileSource.h
 *
 * Description:
 *   This file defines the interface for tileSource.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	tileSource.h
//  Classes				:	CTileLevelInfo , CTileSource
//  Description			:	Format-agnostic tile-fetch interface for the
//							runtime texture read path (render/shading time)
//
////////////////////////////////////////////////////////////////////////
#ifndef TILESOURCE_H
#define TILESOURCE_H

///////////////////////////////////////////////////////////////////////
// Class				:	CTileLevelInfo
// Description			:	One mip level's geometry/format, populated by
//							CTileSource::info()
// Comments				:
struct CTileLevelInfo {
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int numChannels = 0;
    int bitsPerSample = 0; // 8, 16, or 32
    bool isFloatFormat = false;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTileSource
// Description			:	Format-agnostic tile-fetch interface. One
//							instance represents exactly one already-known
//							mip level of one texture -- a multi-level
//							texture is one CTileSource instance per level,
//							not one instance serving all levels. No
//							colorspace/gamma conversion, no resampling --
//							samples are delivered exactly as stored, in the
//							native precision info() reports.
// Comments				:
class CTileSource {
    public:
        virtual ~CTileSource() {}

        // Populates info with this instance's single level's geometry.
        virtual void info(CTileLevelInfo &info) = 0;

        // Fetches one full, tile-aligned tile into dest, a caller-allocated
        // buffer sized as tileWidth * tileHeight * numChannels *
        // (bitsPerSample / 8) bytes (per the info() this instance reports).
        // For a non-tiled (flat/un-made) source, tileX/tileY are always
        // (0, 0) and the "tile" is the entire image. Returns false on any
        // I/O or decode failure -- never a partial or garbage-filled
        // buffer, and never a crash.
        //
        // Safe to call from a different thread than the one that
        // constructed this instance, and safe when a DIFFERENT
        // CTileSource instance's fetchTile() is called concurrently from
        // another thread. NOT required to guard against two threads
        // calling fetchTile() on the SAME instance for the SAME tile
        // concurrently -- the caller (the per-CTextureBlock cache-slot
        // lock, unchanged by this spec) already guarantees that never
        // happens.
        virtual bool fetchTile(int tileX, int tileY, void *dest) = 0;
};

// Returns a new CTileSource reading directory "directory" of the baked TIFF
// texture at filename. Ownership transfers to the caller. The concrete
// backend class (CTiffTileSource) is file-local to texture.cpp -- this
// factory is the only way anything outside that file can construct one, for
// the one legitimate outside use this spec has (direct unit testing of
// info(), which has no in-tree production caller of its own -- see
// tests/unit/texture_tile/test_tile_source_tiff_info.cpp).
CTileSource *createTiffTileSource(const char *filename, short directory);

// Decodes filename via CImageInput, builds the full in-memory mip
// pyramid (resizing to a power of two first if needed), and returns one
// CTileSource instance for level "level" of that pyramid. Returns
// nullptr if filename cannot be decoded by any supported CImageInput
// backend, on any decode failure, or if "level" is out of range. Spec
// 020 (020-runtime-tile-paging). Ownership of the returned CTileSource
// transfers to the caller.
//
// Unlike createTiffTileSource() (cheap -- just stores a filename+
// directory, no I/O until fetchTile()), this factory does real decode
// work on every call: it is meant for the one legitimate outside use
// this spec has (direct unit testing of info()/fetchTile(), mirroring
// createTiffTileSource()'s own rationale), not for constructing every
// level of a real texture load -- CRenderer::textureLoad()'s own
// fallback branch decodes once and constructs all levels together via a
// file-local helper in texture.cpp, per
// contracts/synthesized-tile-source.md.
CTileSource *createSynthesizedTileSource(const char *filename, int level);

#endif
