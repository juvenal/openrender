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
#include "dso.h"
#include "error.h"
#include "includes/logging.hpp"
#include "irradiance.h"
#include "netFileMapping.h"
#include "options.h"
#include "photonMap.h"
#include "pointCloud.h"
#include "pointHierarchy.h"
#include "remoteChannel.h"
#include "renderer.h"
#include "rendererContext.h"
#include "shadeop.h"
#include "texture.h"
#include "texture3d.h"

// This one is defined in rslo.y
CShader *parseShader(const char *, const char *);

// =========================================================================
// parseSloShader — load a CShader from a standalone .slo LLVM bitcode file.
// Extracts named metadata (C3 format) to populate shader type, parameters,
// and varyingSizes without requiring a companion .rslo file.
// Returns nullptr if the .slo has no embedded openrender metadata (pre-C3 .slo).
// =========================================================================
#ifdef OPENRENDER_HAVE_LLVM
#include "config.h"
#include "libshader/shading/llvmJit.h"
#include "libshader/shading/sloMetadata.h"
#include "rslo_code.h" // SL_VARYING_OPERAND
#include "shader.h"    // CShader, SL_SURFACE, SL_LIGHTSOURCE, …
#include "variable.h"  // CVariable, EVariableType, EVariableStorage
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Map SLOShaderInfo typeName to SL_* constant.
static unsigned int sloShaderType(const std::string &n) {
    if (n == "light")
        return SL_LIGHTSOURCE;
    if (n == "displacement")
        return SL_DISPLACEMENT;
    if (n == "atmosphere")
        return SL_ATMOSPHERE;
    if (n == "imager")
        return SL_IMAGER;
    return SL_SURFACE;
}

// Float count per type element (for varyingSizes computation).
static int sloFloatCount(const std::string &t) {
    if (t == "vector" || t == "normal" || t == "point" || t == "color")
        return 3;
    if (t == "matrix")
        return 16;
    return 1; // float, string (string uses char* but we keep 1 for sizing)
}

// Map SLOParamInfo typeName → EVariableType.
static EVariableType sloVarType(const std::string &t) {
    if (t == "color")
        return TYPE_COLOR;
    if (t == "vector")
        return TYPE_VECTOR;
    if (t == "normal")
        return TYPE_NORMAL;
    if (t == "point")
        return TYPE_POINT;
    if (t == "matrix")
        return TYPE_MATRIX;
    if (t == "string")
        return TYPE_STRING;
    return TYPE_FLOAT;
}

static CShader *parseSloShader(const char * /*shaderName*/, const char *sloPath) {
    SLOShaderInfo info;
    if (!CLLVMJitEngine::extractMetadataFromFile(sloPath, info) || !info.valid())
        return nullptr; // no embedded metadata → fall back to .rslo

    // The CShader's CFileResource::name stores the FULL .slo path so that
    // CProgrammableShaderInstance::prepare() can locate and JIT-compile it.
    // getShader() caches this CShader under the logical shader name, not this path.
    CShader *sh = new CShader(sloPath);
    sh->type = sloShaderType(info.typeName);

    int numParams = (int)info.params.size();
    int numVars = (int)info.vars.size();
    int numTotal = numParams + numVars;

    // --- varyingSizes[] covers ALL slots (params [0..numParams-1] then vars).
    // Mirrors rslo.y: -(numItems*numComp*sizeof(float)) for uniform, positive for varying.
    sh->numVariables = numTotal;
    if (numTotal > 0) {
        sh->varyingSizes = (int *)malloc(numTotal * sizeof(int));
        int idx = 0;
        auto fillSize = [&](const SLOParamInfo &p) {
            int sz = p.arraySize * sloFloatCount(p.typeName) * (int)sizeof(float);
            sh->varyingSizes[idx++] = (p.storage == "uniform") ? -sz : sz;
        };
        for (const auto &p : info.params)
            fillSize(p);
        for (const auto &v : info.vars)
            fillSize(v);
    }

    // --- parameters linked list: ONLY actual shader parameters (Ka, Kd, …), NOT locals.
    // Local temporaries are accessed exclusively by slot index through locals[] at execute time.
    // The list is built in reverse source order (matching rslo.y's prepend convention),
    // so the final order mirrors what the interpreter expects.
    // defaultValue MUST be allocated with new float[] to match CShader::~CShader()'s delete[].
    CVariable *paramHead = nullptr;
    for (int i = numParams - 1; i >= 0; --i) {
        const SLOParamInfo &p = info.params[i];
        int totalFloats = p.arraySize * sloFloatCount(p.typeName);

        CVariable *v = new CVariable;
        strncpy(v->name, p.name.c_str(), VARIABLE_NAME_LENGTH - 1);
        v->name[VARIABLE_NAME_LENGTH - 1] = '\0';
        v->type = sloVarType(p.typeName);
        v->container = (p.storage == "varying") ? CONTAINER_VARYING : CONTAINER_UNIFORM;
        v->storage = STORAGE_PARAMETER;
        v->numItems = p.arraySize;
        v->numFloats = totalFloats;
        v->entry = i; // slot i in varyingSizes[] and locals[]
        v->accessor = SL_VARYING_OPERAND;
        v->usageMarker = 0;

        // Allocate and populate default value using new[] (matched by delete[] in CShader dtor).
        v->defaultValue = new float[totalFloats > 0 ? totalFloats : 1];
        float *buf = (float *)v->defaultValue;
        for (int j = 0; j < totalFloats; j++)
            buf[j] = 0.0f;
        if (!p.defaultStr.empty()) {
            // Reuse parseDefaultStr's logic inline to avoid malloc/new mismatch.
            const char *cp = p.defaultStr.c_str();
            while (*cp == ' ' || *cp == '[')
                ++cp;
            int numParsed = 0;
            for (int j = 0; j < totalFloats; j++) {
                char *end = nullptr;
                buf[j] = strtof(cp, &end);
                if (!end || end == cp)
                    break;
                ++numParsed;
                cp = end;
                while (*cp == ' ' || *cp == ']')
                    ++cp;
            }
            // RSL scalar-to-tuple broadcast: "color = 1" means {1,1,1}, "vector = 0" means {0,0,0}.
            if (numParsed == 1 && totalFloats > 1) {
                for (int j = 1; j < totalFloats; j++)
                    buf[j] = buf[0];
            }
        }

        v->next = paramHead;
        paramHead = v;
    }
    sh->parameters = paramHead;
    sh->numGlobals = 0;

    // Interpreter fields null — JIT is the only execution path for .slo-based shaders.
    sh->memory = nullptr;
    sh->codeArea = nullptr;
    sh->constantEntries = nullptr;
    sh->strings = nullptr;
    sh->numStrings = 0;
    sh->codeEntryPoint = 0;
    sh->initEntryPoint = 0;
    sh->usedParameters = info.usedParameters;
    sh->flags = 0;
    sh->data = nullptr;

    sh->analyse(); // populates flags / CLightShaderData for light shaders

    // Eagerly compile the JIT so that jitInitEntry is available when init() is
    // called at shader-bind time (before the first prepare() is ever called).
    {
        const char *filename = sloPath;
        const char *sep = strrchr(filename, OS_DIR_SEPERATOR);
        const char *base = sep ? sep + 1 : filename;
        const char *ext = strrchr(base, '.');
        std::string logicalName = ext ? std::string(base, ext) : std::string(base);
        sh->jitEntry = CLLVMJitEngine::getInstance().compileShader(sloPath, logicalName);
        sh->jitInitEntry = CLLVMJitEngine::getInstance().lookupInitEntry(logicalName);
    }

    return sh;
}
#endif // OPENRENDER_HAVE_LLVM

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

    // DSO init
    dsos = NULL;
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

    // Ditch the DSO shaders that have been loaded
    CDSO *cDso;
    for (cDso = dsos; cDso != NULL;) {
        CDSO *nDso = cDso->next;
        // Call DSO's cleanup
        if (cDso->cleanup != NULL)
            cDso->cleanup(cDso->handle);
        free(cDso->name);
        free(cDso->prototype);
        delete cDso;
        cDso = nDso;
    }

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

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getTexture
// Description			:	Load a texture from file
// Return Value			:
// Comments				:
CTexture *CRenderer::getTexture(const char *name) {
    CFileResource *tex;

    assert(name != NULL);
    assert(frameFiles != NULL);

    if (frameFiles->find(name, tex) == FALSE) {

        // Load the texture
        tex = textureLoad(name, texturePath);

        if (tex == NULL) {
            // Not found, substitude with a dummy one
            if (*name != 0)
                error(CODE_NOFILE, "Failed open texture \"%s\"\n", name);
            tex = new CDummyTexture(name);
        }

        frameFiles->insert(tex->name, tex);
    }

    return (CTexture *)tex;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getEnvironment
// Description			:	Load an environment map (which can also be a shadow map)
// Return Value			:
// Comments				:
CEnvironment *CRenderer::getEnvironment(const char *name) {
    CFileResource *tex;

    assert(name != NULL);
    assert(frameFiles != NULL);

    if (frameFiles->find(name, tex) == FALSE) {

        tex = environmentLoad(name, texturePath, toWorld);

        if (tex == NULL) {
            // Not found, substitude with a dummy one
            error(CODE_NOFILE, "Failed open environment \"%s\"\n", name);
            tex = new CDummyEnvironment(name);
        }

        frameFiles->insert(tex->name, tex);
    }

    return (CEnvironment *)tex;
}

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
        map = new CPhotonMap(name, in);
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

    assert(name != NULL);
    assert(frameFiles != NULL);

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

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getTextureInfo
// Description			:	Load a texture from file
// Return Value			:
// Comments				:
CTextureInfoBase *CRenderer::getTextureInfo(const char *name) {
    CFileResource *tex;

    assert(name != NULL);
    assert(frameFiles != NULL);

    if (frameFiles->find(name, tex) == FALSE) {
        // try environments first
        tex = environmentLoad(name, texturePath, toWorld);

        if (tex == NULL) {
            // else try as textures
            tex = textureLoad(name, texturePath);
        }

        if (tex != NULL) {
            // only store the result if found
            frameFiles->insert(tex->name, tex);
        }
    }

    return (CTextureInfoBase *)tex;
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
                        texture3d = new CPointHierarchy(name, from, to, in);
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
// Function				:	dsoLoadCallback
// Description			:	This function will be called for each module
// Return Value			:
// Comments				:
static int dsoLoadCallback(const char *file, void *ud) {
    void *module = osLoadModule(file);

    if (module != NULL) {
        int i;
        void **userData = (void **)ud;
        char *name = (char *)userData[0];
        char *prototype = (char *)userData[1];
        SHADEOP_SPEC *shadeops;

        {
            char tmp[OS_MAX_PATH_LENGTH];

            snprintf(tmp, sizeof(tmp), "%s_shadeops", name);

            shadeops = (SHADEOP_SPEC *)osResolve(module, tmp);
        }

        if (shadeops != NULL) {
            for (i = 0;; i++) {
                char *dsoName, *dsoPrototype;

                if (strcmp(shadeops[i].definition, "") == 0)
                    break;

                if (dsoParse(shadeops[i].definition, dsoName, dsoPrototype) == TRUE) {
                    if (strcmp(dsoPrototype, prototype) == 0) {
                        dsoInitFunction *init = (dsoInitFunction *)userData[2];
                        dsoExecFunction *exec = (dsoExecFunction *)userData[3];
                        dsoCleanupFunction *cleanup = (dsoCleanupFunction *)userData[4];

                        // Bingo
                        init[0] = (dsoInitFunction)osResolve(module, shadeops[i].init);
                        exec[0] = (dsoExecFunction)osResolve(module, dsoName);
                        cleanup[0] = (dsoCleanupFunction)osResolve(module, shadeops[i].cleanup);

                        if (exec != NULL) {
                            free(dsoName);
                            free(dsoPrototype);

                            // We have found the DSO
                            return FALSE;
                        }
                    }

                    free(dsoName);
                    free(dsoPrototype);
                }
            }
        }

        osUnloadModule(module);
    }
    else {
        error(CODE_SYSTEM, "Failed to load DSO \"%s\": %s\n", file, osModuleError());
    }

    // Continue iterating
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getDSO
// Description			:	Load a DSO matching the prototyoe
// Return Value			:
// Comments				:	This function does not need to be thread safe
CDSO *CRenderer::getDSO(const char *name, const char *prototype) {
    CDSO *cDso;

    assert(name != NULL);

    // Check if the DSO had been loaded before
    for (cDso = dsos; cDso != NULL; cDso = cDso->next) {
        if (strcmp(cDso->name, name) == 0) {
            if (strcmp(cDso->prototype, prototype) == 0) {
                return cDso;
            }
        }
    }

    dsoInitFunction init;
    dsoExecFunction exec;
    dsoCleanupFunction cleanup;

    init = NULL;
    exec = NULL;
    cleanup = NULL;

    void *userData[5];
    userData[0] = (void *)name;
    userData[1] = (void *)prototype;
    userData[2] = &init;
    userData[3] = &exec;
    userData[4] = &cleanup;

    // Go over the directories
    TSearchpath *inPath = proceduralPath;
    char searchPath[OS_MAX_PATH_LENGTH];
    for (; inPath != NULL; inPath = inPath->next) {
        snprintf(searchPath, sizeof(searchPath), "%s*.%s", inPath->directory, osModuleExtension);
        osEnumerate(searchPath, dsoLoadCallback, userData);
    }

    if (exec != NULL) {
        void *handle;

        // OK, we found the shader
        if (init != NULL)
            handle = init(0, NULL);
        else
            handle = NULL;

        // Save the DSO
        cDso = new CDSO;
        cDso->init = init;
        cDso->exec = exec;
        cDso->cleanup = cleanup;
        cDso->handle = handle;
        cDso->name = strdup(name);
        cDso->prototype = strdup(prototype);
        cDso->next = CRenderer::dsos;
        CRenderer::dsos = cDso;

        return cDso;
    }

    return NULL;
}
