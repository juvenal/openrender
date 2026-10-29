/**
 * Project: openRender
 *
 * File: textureResolver.h
 *
 * Description:
 *   The way Texturing finds the files it reads (spec 027, FR-007). Texturing may not call the
 *   renderer core, so a creator supplies a resolver: the renderer core's is CSearchPathResolver
 *   (src/ri/render/searchPathResolver.h), over a search path, and a test supplies its own.
 *
 *   A nullptr resolver given to a make* function or a loader means "no search path": the name is used as
 *   given (a path, or relative to the working directory) when the file exists, and nothing else is searched.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef TEXTURERESOLVER_H
#define TEXTURERESOLVER_H

#include <string.h>

#include "common/global.h"
#include "common/os.h"

// Finds texture-related files for Texturing, which may not call the renderer core
class CTextureFileResolver {
    public:
        virtual ~CTextureFileResolver() = default;

        // Writes the full path of `name` into `result` (OS_MAX_PATH_LENGTH bytes) and returns TRUE
        // when the file exists; FALSE otherwise. Must be callable from several threads at once.
        virtual int locate(char *result, const char *name) const = 0;
};

// Locates `name` through `resolver`, which may be nullptr ("no search path", see above).
// Returns TRUE and fills `result` (OS_MAX_PATH_LENGTH bytes) when the file was found.
inline int textureLocate(const CTextureFileResolver *resolver, char *result, const char *name) {
    if (resolver != nullptr)
        return resolver->locate(result, name);

    // No search path: the name is used as given (a directory separator makes it a path; a bare name is
    // relative to the working directory) when the file exists. This is what CRenderer::locateFile does
    // with an empty search path and an empty temporary directory, which is how
    // tests/preview/test_data_keys.cpp reaches makeBrickMap() (a bare name, NULL path).
    if (name != nullptr && name[0] != '\0' && osFileExists(name)) {
        strncpy(result, name, OS_MAX_PATH_LENGTH - 1);
        result[OS_MAX_PATH_LENGTH - 1] = '\0';
        return TRUE;
    }
    return FALSE;
}

#endif
