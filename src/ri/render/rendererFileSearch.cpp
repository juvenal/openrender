/**
 * Project: openRender
 *
 * File: rendererFileSearch.cpp
 *
 * Description:
 *   CRenderer::normalizeFileName()/locateFile() -- pure filesystem/network
 *   path resolution, with no shading-engine or Reyes-hider dependency of
 *   their own. Split out of render/rendererFiles.cpp (whose OTHER methods
 *   -- getShader/getPhotonMap/getCache/getTexture3d, needing LLVM's
 *   CLLVMJitEngine and constructing CProgrammableShaderInstance/CShader --
 *   are full-pipeline-only and stay excluded from ribVector) because
 *   locateFile() itself is genuinely needed broadly: blobbyRepeller.cpp,
 *   texmake.cpp, texture.cpp, brickmap.cpp all call it directly, and the
 *   RIB grammar (rib_generated.cpp, via RiReadArchiveV's archive-path
 *   search) calls it too -- none of that is full-pipeline-specific, so
 *   excluding the whole file would have broken real functionality rather
 *   than shedding dead weight.
 *
 *   Compiled identically for every consumer (ribRender and ribVector
 *   alike) -- there is no real/stub split here, unlike geometryDispatch.cpp
 *   or irradianceDispatch.cpp, because nothing in this code touches
 *   CShadingContext/CReyes/LLVM to begin with.
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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "netFileMapping.h"
#include "options.h"
#include "renderer.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	normalizeFileName
// Description			:	Make sure there are no funny characters in the name
// Return Value			:	TRUE if found
// Comments				:
int CRenderer::normalizeFileName(char *name) {
    int normalized = FALSE;

    // Normalize the file name
    for (; *name != '\0'; ++name) {
        if ((*name == '/') || (*name == '\\')) {
            *name = '_';
            normalized = TRUE;
        }
    }

    // return value
    return normalized;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	locateFile
// Description			:	Locate a file on disk
// Return Value			:	TRUE if found
// Comments				:
int CRenderer::locateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize) {

    if (netClient != INVALID_SOCKET) {
        // check netfile mappings
        CNetFileMapping *mapping;
        if (netFileMappings->find(name, mapping)) {
            name = mapping->to;
        }
    }

    if (strchr(name, OS_DIR_SEPERATOR)) {
        // Supplied path
        // Check if the file exists
        if (osFileExists(name)) {
            strcpy(result, name);
            info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, name);
            return TRUE;
        }
    }
    else {
        // Only filename
        // Look at the search path
        for (; searchpath != NULL; searchpath = searchpath->next) {
            snprintf(result, OS_MAX_PATH_LENGTH, "%s%s", searchpath->directory, name);
            osFixSlashes(result);
            if (osFileExists(result)) {
                info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, result);
                return TRUE;
            }
        }

        // Last resort, look into the temporary directory
        snprintf(result, OS_MAX_PATH_LENGTH, "%s%s", temporaryPath, name);
        osFixSlashes(result);
        if (osFileExists(result)) {
            info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, result);
            return TRUE;
        }
    }

    // Unable to find the file, check the network
    // Check the net if we can find the file
    if (netClient != INVALID_SOCKET) {

        // Lock the network
        osLock(networkMutex);

        if (getFile(result, name) == TRUE) {
            if (osFileExists(result)) {
                info(CODE_RESOLUTION, "\"%s\" -> \"%s\"\n", name, result);
                osUnlock(networkMutex);
                return TRUE;
            }
        }

        // Unlock the network
        osUnlock(networkMutex);
    }

    // Should we check normalized?
    if (tryNormalize) {
        char normalizedName[OS_MAX_PATH_LENGTH];

        // Normalize the file name
        strcpy(normalizedName, name);
        CRenderer::normalizeFileName(normalizedName);

        // Search again
        return locateFile(result, normalizedName, searchpath, FALSE);
    }

    info(CODE_RESOLUTION, "\"%s\" -> ???\n", name);

    return FALSE;
}
