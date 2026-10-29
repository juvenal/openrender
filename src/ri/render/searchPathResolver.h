/**
 * Project: openRender
 *
 * File: searchPathResolver.h
 *
 * Description:
 *   The renderer core's file resolver for Texturing (spec 027, FR-007): a CTextureFileResolver over a
 *   TSearchpath, calling CRenderer::locateFile(), so the search order, the network mapping, the
 *   temporary-directory fallback and the CODE_RESOLUTION messages are those of every other file lookup.
 *   Defined in rendererFileSearch.cpp beside CRenderer::locateFile (compiled into ribRender and ribVector).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef SEARCHPATHRESOLVER_H
#define SEARCHPATHRESOLVER_H

#include "texture/textureResolver.h"

struct TSearchpath;

class CSearchPathResolver : public CTextureFileResolver {
    public:
        explicit CSearchPathResolver(TSearchpath *searchPath) : path(searchPath) {}

        int locate(char *result, const char *name) const override;

    private:
        TSearchpath *path; // not owned; outlives the resolver
};

#endif
