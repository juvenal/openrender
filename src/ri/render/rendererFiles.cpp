/**
 * Project: openRender
 *
 * File: rendererFiles.cpp
 *
 * Description:
 *   This file implements the functionality for rendererFiles.
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
//  File				:	rendererFiles.cpp
//  Classes				:	CRenderer
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <string.h>

#include "brickmap.h"
#include "common/diagnostics.h"
#include "include/logging.hpp"
#include "irradiance.h"
#include "netFileMapping.h"
#include "options.h"
#include "photonMap.h"
#include "pointCloud.h"
#include "pointHierarchy.h"
#include "remoteChannel.h"
#include "renderer.h"
#include "rendererContext.h"
#include "searchPathResolver.h"
#include "shadeop/shadeopModule.h"
#include "shading.h"
#include "libshader/shading/dsoHandle.h" // CDSO
#include "texture.h"
#include "texture3d.h"

// This one is defined in rslo.y
CShader *parseShader(const char *, const char *);

#include "libshader/shading/sloLoader.h" // parseSloShader() (moved into Shading, spec 026)


///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	initFiles
// Description			:	Init the files
// Return Value			:
// Comments				:
void CRenderer::initFiles() {

    // The loaded shaders
    globalFiles = new CTrie<CFileResource *>;

    // Temporary files we store per frame
    frameTemporaryFiles = NULL;

    // The shadeop operands (spec 037): one registry per RiBegin..RiEnd; beginFrame() sizes its per-thread slots
    shadeops = new CShadeopRegistry;
}

///////////////////////////////////////////////////////////////////////
// Function				:	sfClearTemp
// Description			:	This callback function is used to remove the temporary files
// Return Value			:
// Comments				:
static int rcClearTemp(const char *fileName, void *) {
    osDeleteFile(fileName);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	shutdownFiles
// Description			:	Shutdown the files
// Return Value			:
// Comments				:
void CRenderer::shutdownFiles() {

    // Ditch the temporary files created
    if (osFileExists(temporaryPath)) {
        char tmp[OS_MAX_PATH_LENGTH + 1];

        snprintf(tmp, sizeof(tmp), "%s*", temporaryPath);
        osFixSlashes(tmp);
        osEnumerate(tmp, rcClearTemp, NULL);
        osDeleteDir(temporaryPath);
    }

    // Ditch the shadeop operands that have been loaded: per operand the per-thread cleanups, the cleanup and
    // the unload, in that order (FR-011 d, FR-034)
    delete shadeops;
    shadeops = NULL;

    // Ditch the loaded files
    assert(globalFiles != NULL);
    globalFiles->destroy();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	locateFileEx
// Description			:	Locate a file on disk
// Return Value			:	TRUE if found
// Comments				:
int CRenderer::locateFileEx(char *result, const char *name, const char *extension, TSearchpath *searchpath) {
    const char *dotpos = strchr(name, '.');
    const char *seppos = strchr(name, OS_DIR_SEPERATOR);
    if (dotpos == NULL || (seppos != NULL && dotpos < seppos)) {
        char tmp[OS_MAX_PATH_LENGTH];

        snprintf(tmp, sizeof(tmp), "%s.%s", name, extension);

        return locateFile(result, tmp, searchpath);
    }
    else {
        return locateFile(result, name, searchpath);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	normalizeFileName / locateFile
// Comments				:	Moved to render/rendererFileSearch.cpp (no
//							shading-engine dependency of their own, and
//							genuinely needed outside the full pipeline --
//							see that file's header comment).

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getPhotonMap
// Description			:	Load a photon map
// Return Value			:
// Comments				:
CPhotonMap *CRenderer::getPhotonMap(const char *name) {
    CFileResource *map;
    char fileName[OS_MAX_PATH_LENGTH];
    FILE *in;

    assert(name != NULL);
    assert(frameFiles != NULL);

    // Check the cache to see if the file is in the memory
    if (frameFiles->find(name, map) == FALSE) {

        // Locate the file
        if (locateFile(fileName, name, texturePath)) {
            // Try to open the file
            in = ropen(fileName, "rb", filePhotonMap, TRUE);
        }
        else {
            in = NULL;
        }

        // Read it
        // The map's ctor counts the cache's reference; endFrame() releases
        // it through framePhotonMaps rather than deleting the map
        CPhotonMap *photonMap = new CPhotonMap(name, in);
        framePhotonMaps->push(photonMap);
        map = photonMap;
        frameFiles->insert(map->name, map);
    }

    return (CPhotonMap *)map;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getCache
// Description			:	Load a cache
// Return Value			:
// Comments				:
CTexture3d *CRenderer::getCache(const char *name, const char *mode, const float *from, const float *to) {
    CFileResource *cache;

    assert(frameFiles != NULL);

    // name/mode ultimately come from RIB-authored attribute strings (GitHub
    // #4); NDEBUG strips asserts in Release, so a NULL here must be handled,
    // not just documented. Fall back to the same empty-string representation
    // CAttributes's own defaults use -- "" degrades to an in-memory-only
    // cache below, and an unrecognized mode already falls back to
    // CACHE_SAMPLE a few lines down.
    if (name == NULL)
        name = "";
    if (mode == NULL)
        mode = "";

    // Check the memory first
    if (frameFiles->find(name, cache) == FALSE) {
        char fileName[OS_MAX_PATH_LENGTH];
        int flags;
        char type[128];
        int createChannel = FALSE;

        // Process the file mode
        if (strcmp(mode, "r") == 0) {
            flags = CACHE_READ | CACHE_SAMPLE;
        }
        else if (strcmp(mode, "w") == 0) {
            flags = CACHE_WRITE | CACHE_SAMPLE;
        }
        else if (strcmp(mode, "R") == 0) {
            flags = CACHE_READ | CACHE_RDONLY;
        }
        else if (strcmp(mode, "rw") == 0) {
            flags = CACHE_READ | CACHE_WRITE | CACHE_SAMPLE;
        }
        else {
            flags = CACHE_SAMPLE;
        }

        // Try to read the file
        cache = NULL;
        if (flags & CACHE_READ) {

            // Locate the file
            if (locateFile(fileName, name, texturePath)) {
                FILE *in = ropen(fileName, type);

                if (in != NULL) {
                    // If we're netrendering and writing, treat specially
                    if ((netClient != INVALID_SOCKET) && (flags & CACHE_WRITE)) {
                        flags &= ~CACHE_WRITE; // don't flush cache to disk
                        createChannel = TRUE;
                        if (strncmp(fileName, temporaryPath, strlen(temporaryPath)) == 0) {
                            // it's a temp file, delete it after we're done
                            registerFrameTemporary(fileName, TRUE);
                        }
                        // always remove the file mapping when writing
                        registerFrameTemporary(name, FALSE);
                    }

                    // Create the cache
                    if (strcmp(type, fileIrradianceCache) == 0) {
                        cache = new CIrradianceCache(name, flags, in, from, to, NULL);
                    }
                    else {
                        error(CODE_BUG, "Unable to recognize the file format of \"%s\"\n", name);
                        fclose(in);
                    }
                }
            }
        }

        // If there is no cache, create it
        if (cache == NULL) {
            // If we're netrendering and writing, treat specially
            if ((netClient != INVALID_SOCKET) && (flags & CACHE_WRITE)) {
                flags &= ~CACHE_WRITE; // don't flush cache to
                createChannel = TRUE;
                // always remove the file mapping when writing
                registerFrameTemporary(name, FALSE);
            }

            // go ahead and create the cache
            cache = new CIrradianceCache(name, flags, NULL, from, to, CRenderer::toNDC);
        }

        // Create channels if possible
        if (createChannel == TRUE) {
            if (cache != NULL) {
                requestRemoteChannel(new CRemoteICacheChannel((CIrradianceCache *)cache));
            }
        }

        frameFiles->insert(cache->name, cache);
    }

    return (CTexture3d *)cache;
}

// The point hierarchy's solid-angle limit: the occlusion() "maxsolidangle" of the lookup in progress (spec 027)
static float pointHierarchyMaxSolidAngle(const CShadingContext *context) {
    return context->currentShadingState->scratch.occlusionParams.maxSolidAngle;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getTexture3d
// Description			:	Get a point cloud or brickmap
// Return Value			:
// Comments				:
CTexture3d *CRenderer::getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy) {
    CFileResource *texture3d;
    char fileName[OS_MAX_PATH_LENGTH];
    FILE *in;

    assert(name != NULL);
    assert(frameFiles != NULL);

    if (frameFiles->find(name, texture3d) == FALSE) {

        if (from == NULL) {
            from = world->from;
            to = world->to;
        }

        // If we are writing, it must be a point cloud
        if (write == TRUE) {

            if (netClient != INVALID_SOCKET) {
                CPointCloud *cloud = new CPointCloud(name, world->from, world->to, CRenderer::toNDC, channels, FALSE);
                texture3d = cloud;

                // Ensure we unmap the file when done.  Do not delete it
                // as we mark the file to never be written in the server
                registerFrameTemporary(name, FALSE);
                requestRemoteChannel(new CRemotePtCloudChannel(cloud));
            }
            else {
                // alloate a point cloud which will be written to disk
                texture3d = new CPointCloud(name, from, to, CRenderer::toNDC, channels, TRUE);
            }
        }
        else {
            // Locate the file
            if (locateFile(fileName, name, texturePath)) {
                // Try to open the file
                if ((in = ropen(fileName, "rb", filePointCloud, TRUE)) != NULL) {
                    if (hierarchy == TRUE) {
                        CPointHierarchy::Options options;
                        options.maxSolidAngle = pointHierarchyMaxSolidAngle;
                        texture3d = new CPointHierarchy(name, from, to, in, options);
                    }
                    else {
                        texture3d = new CPointCloud(name, from, to, in);
                    }
                }
                else {
                    if ((in = ropen(fileName, "rb", fileBrickMap, TRUE)) != NULL) {
                        texture3d = new CBrickMap(in, name, from, to);
                    }
                }
            }
            else {
                in = NULL;
            }

            if (in == NULL) {
                // allocate a dummy blank-channel point cloud
                error(CODE_BADTOKEN, "Cannot find or open Texture3D file \"%s\"\n", name);
                texture3d = new CPointCloud(name, world->from, world->to, NULL, NULL, FALSE);
                // remove the dummy mapping once the frame ends
                registerFrameTemporary(name, FALSE);
            }
        }

        frameFiles->insert(texture3d->name, texture3d);
    }

    return (CPointCloud *)texture3d;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRendererContext
// Method				:	getShader
// Description			:	Create an instance of a shader
// Return Value			:
// Comments				:
CShader *CRenderer::getShader(const char *name, TSearchpath *path) {
    return getShader(name, path, OPENRENDER_DEFAULT_FORMAT); // Default preference
}

CShader *CRenderer::getShader(const char *name, TSearchpath *path, const char *preferredType) {
    CShader *cShader;
    CFileResource *file;

    assert(name != NULL);
    assert(globalFiles != NULL);

    // Check if we already loaded this shader before ...
    cShader = NULL;
    if (globalFiles->find(name, file)) {
        cShader = (CShader *)file;
    }
    else {
        char shaderLocation[OS_MAX_PATH_LENGTH];

        // Validate preference
        const bool preferSlo = (preferredType != NULL && strcmp(preferredType, "slo") == 0);

        if (preferSlo) {
            // Try .slo first, fall back to .rslo
#ifdef OPENRENDER_HAVE_LLVM
            if (CRenderer::locateFileEx(shaderLocation, name, "slo", path) == TRUE) {
                cShader = parseSloShader(name, shaderLocation);
            }
#else
            // The scene asked for the JIT backend but this binary has none --
            // LLVM was missing, or older than OPENRENDER_LLVM_MIN_VERSION, when
            // it was configured. The .rslo fallback below still renders, so say
            // so once rather than letting a build-configuration mismatch look
            // like a normal render. Only the no-JIT-at-all case warns; a build
            // *with* the JIT that simply has no .slo on disk falls back
            // silently, which is a legitimate and intentional path.
            warning(CODE_INCAPABLE,
                    "Shader \"%s\": shaderformat \"slo\" requested, but this build has no "
                    "LLVM JIT support; using the .rslo interpreter instead\n",
                    name);
#endif
            if (cShader == NULL &&
                CRenderer::locateFileEx(shaderLocation, name, "rslo", path) == TRUE) {
                cShader = parseShader(name, shaderLocation);
            }
        }
        else {
            // Try .rslo first, fall back to .slo
            if (CRenderer::locateFileEx(shaderLocation, name, "rslo", path) == TRUE) {
                cShader = parseShader(name, shaderLocation);
            }
#ifdef OPENRENDER_HAVE_LLVM
            if (cShader == NULL &&
                CRenderer::locateFileEx(shaderLocation, name, "slo", path) == TRUE) {
                cShader = parseSloShader(name, shaderLocation);
            }
#endif
        }

        if (cShader != NULL) {
            log_debug("getShader: loaded shader '{}' from '{}'", name, shaderLocation);
            // Cache under cShader->name (a stable, owned copy from CFileResource's
            // ctor), not the caller's `name`: the RIB parser's per-statement memory
            // arena is rewound (memRestore) before the next statement is lexed, so
            // `name` can be silently overwritten once a later statement's tokens
            // reuse the same address, aliasing the trie's cached key.
            globalFiles->insert(cShader->name, cShader);
        }
    }

    return cShader;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getFilter
// Description			:	Return the filter matching the name
// Return Value			:
// Comments				:
RtFilterFunc CRenderer::getFilter(const char *name) {
    if (strcmp(name, RI_GAUSSIANFILTER) == 0) {
        return RiGaussianFilter;
    }
    else if (strcmp(name, RI_BOXFILTER) == 0) {
        return RiBoxFilter;
    }
    else if (strcmp(name, RI_TRIANGLEFILTER) == 0) {
        return RiTriangleFilter;
    }
    else if (strcmp(name, RI_SINCFILTER) == 0) {
        return RiSincFilter;
    }
    else if (strcmp(name, RI_CATMULLROMFILTER) == 0) {
        return RiCatmullRomFilter;
    }
    else if (strcmp(name, RI_BLACKMANHARRISFILTER) == 0) {
        return RiBlackmanHarrisFilter;
    }
    else if (strcmp(name, RI_MITCHELLFILTER) == 0) {
        return RiMitchellFilter;
    }
    else if (strcmp(name, RI_BESSELFILTER) == 0) {
        return RiBesselFilter;
    }
    else if (strcmp(name, RI_DISKFILTER) == 0) {
        return RiDiskFilter;
    }

    return RiGaussianFilter;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getFilter
// Description			:	Return the name matching the filter
// Return Value			:
// Comments				:
const char *CRenderer::getFilter(RtFilterFunc func) {
    if (func == RiGaussianFilter) {
        return RI_GAUSSIANFILTER;
    }
    else if (func == RiBoxFilter) {
        return RI_BOXFILTER;
    }
    else if (func == RiTriangleFilter) {
        return RI_TRIANGLEFILTER;
    }
    else if (func == RiSincFilter) {
        return RI_SINCFILTER;
    }
    else if (func == RiCatmullRomFilter) {
        return RI_CATMULLROMFILTER;
    }
    else if (func == RiBlackmanHarrisFilter) {
        return RI_BLACKMANHARRISFILTER;
    }
    else if (func == RiMitchellFilter) {
        return RI_MITCHELLFILTER;
    }
    else if (func == RiBesselFilter) {
        return RI_BESSELFILTER;
    }
    else if (func == RiDiskFilter) {
        return RI_DISKFILTER;
    }

    return RI_GAUSSIANFILTER;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getStepFilter
// Description			:	Return the filter matching the name
// Return Value			:
// Comments				:
RtStepFilterFunc CRenderer::getStepFilter(const char *name) {
    if (strcmp(name, RI_GAUSSIANFILTER) == 0) {
        return RiGaussianStepFilter;
    }
    else if (strcmp(name, RI_BOXFILTER) == 0) {
        return RiBoxStepFilter;
    }
    else if (strcmp(name, RI_TRIANGLEFILTER) == 0) {
        return RiTriangleStepFilter;
        /*	} else if (strcmp(name,RI_SINCFILTER) == 0) {
                return	RiSincFilter;*/
    }
    else if (strcmp(name, RI_CATMULLROMFILTER) == 0) {
        return RiCatmullRomStepFilter;
        /*	} else if (strcmp(name,RI_BLACKMANHARRISFILTER) == 0) {
                return	RiBlackmanHarrisFilter;*/
    }
    else if (strcmp(name, RI_MITCHELLFILTER) == 0) {
        return RiMitchellStepFilter;
    }

    return RiGaussianStepFilter;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getDSO
// Description			:	Find the shadeop operand matching the prototype
// Return Value			:
// Comments				:	The shadeop module scans the procedural path (FR-013: directories in search-path order,
//							file names lexicographic) and keeps what it selected until RiEnd. The CDSO handed to
//							the shading library is owned by the operand. This function does not need to be thread safe
CDSO *CRenderer::getDSO(const char *name, const char *prototype) {
    assert(name != NULL);

    // The procedural path as the shadeop module takes it: directories that end with a separator
    std::vector<const char *> directories;
    for (TSearchpath *inPath = proceduralPath; inPath != NULL; inPath = inPath->next)
        directories.push_back(inPath->directory);
    const SShadeopSearchPath searchPath = {directories.data(), (int)directories.size()};

    CShadeopOperand *operand = shadeops->acquire(searchPath, name, prototype);
    if (operand == NULL)
        return NULL;

    CDSO *dso = (CDSO *)shadeopOperandHostObject(operand);
    if (dso == NULL) {
        dso = new CDSO{operand, strdup(name), strdup(prototype)};
        setShadeopOperandHostObject(operand, dso, [](void *object) {
            CDSO *released = (CDSO *)object;
            free(released->name);
            free(released->prototype);
            delete released;
        });
    }
    return dso;
}
