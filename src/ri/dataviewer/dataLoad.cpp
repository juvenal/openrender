#include "dataLoad.h"

#include "brickMapView.h"
#include "common/algebra.h"
#include "common/global.h"
#include "debug.h"
#include "common/fileResource.h"
#include "irradianceCacheView.h"
#include "common/memory.h"
#include "photonMap.h"
#include "pointCloudView.h"
#include "renderer.h"
#include "riInterface.h"
#include "riServices.h"

#include <cstdio>
#include <cstring>

///////////////////////////////////////////////////////////////////////
// Function             :   isValidDebugDump
// Description          :   Structurally validate a file as a debug-geometry dump (bmin/bmax
//                          followed by a clean tag/payload stream to exact EOF) without
//                          dispatching any of it to a sink. `in` must be freshly opened;
//                          left at an unspecified position on return either way.
static bool isValidDebugDump(FILE *in) {
    fseek(in, 0, SEEK_END);
    long fileSize = ftell(in);
    fseek(in, 0, SEEK_SET);

    if (fileSize < (long)(6 * sizeof(float)))
        return false;

    float discard6[6];
    if (fread(discard6, sizeof(float), 6, in) != 6)
        return false;

    long pos = (long)(6 * sizeof(float));
    while (pos < fileSize) {
        int tag;
        if (fread(&tag, sizeof(int), 1, in) != 1)
            return false;
        pos += sizeof(int);

        int floatCount;
        switch (tag) {
            case 0:
                floatCount = 3;
                break; // point
            case 1:
                floatCount = 6;
                break; // line
            case 2:
                floatCount = 9;
                break; // triangle
            case 3:
                floatCount = 12;
                break; // quad
            default:
                return false;
        }

        long recordBytes = (long)floatCount * sizeof(float);
        if (pos + recordBytes > fileSize)
            return false;

        float discard[12];
        if (fread(discard, sizeof(float), floatCount, in) != (size_t)floatCount)
            return false;
        pos += recordBytes;
    }

    return pos == fileSize;
}

///////////////////////////////////////////////////////////////////////
// Function             :   sniffHeader
// Description          :   Reads and validates the common magic/version/wordsize/type-string
//                          header (ported from the deleted src/ri/show.cpp:82-99, with the
//                          version check corrected to the strict AND-based test ropen() always
//                          actually enforced -- show.cpp's own OR-based local check was inert
//                          dead validation, never the real gatekeeper). On a magic-number
//                          match, `in` is left positioned at the start of that type's payload.
//                          On a debug-dump result (magic mismatch), `in` is left at offset 0,
//                          since CDebugView's FILE* constructor reads its own bmin/bmax from
//                          the very start of the file.
static EDataFileType sniffHeader(FILE *in) {
    unsigned int magic = 0;
    if (fread(&magic, sizeof(int), 1, in) != 1 || magic != magicNumber) {
        fseek(in, 0, SEEK_SET);
        return isValidDebugDump(in) ? DATA_DEBUGDUMP : DATA_NOT_A_DATA_FILE;
    }

    int version[4];
    if (fread(version, sizeof(int), 4, in) != 4)
        return DATA_NOT_A_DATA_FILE;

    if ((version[0] != VERSION_MAJOR) || (version[1] != VERSION_MINOR))
        return DATA_BAD_VERSION;

    if (version[3] != (int)sizeof(int *))
        return DATA_BAD_WORDSIZE;

    int len;
    if (fread(&len, sizeof(int), 1, in) != 1 || len < 0 || len >= 256)
        return DATA_NOT_A_DATA_FILE;

    char type[256];
    if (fread(type, sizeof(char), len + 1, in) != (size_t)(len + 1))
        return DATA_NOT_A_DATA_FILE;

    if (strcmp(type, filePhotonMap) == 0)
        return DATA_PHOTONMAP;
    if (strcmp(type, fileIrradianceCache) == 0)
        return DATA_IRRADIANCECACHE;
    if (strcmp(type, fileGatherCache) == 0)
        return DATA_GATHERCACHE;
    if (strcmp(type, filePointCloud) == 0)
        return DATA_POINTCLOUD;
    if (strcmp(type, fileBrickMap) == 0)
        return DATA_BRICKMAP;

    return DATA_NOT_A_DATA_FILE; // magic + version matched, but an unrecognized type string
}

EDataFileType dataSniff(const char *fileName) {
    FILE *in = fopen(fileName, "rb");
    if (in == NULL)
        return DATA_NOT_A_DATA_FILE;

    EDataFileType type = sniffHeader(in);
    fclose(in);
    return type;
}

CDataDocument::CDataDocument() : dataView(NULL), session(NULL), sink(NULL) {
}

CDataDocument::~CDataDocument() {
    delete dataView;

    if (session != NULL) {
        delete sink; // uninstalls it
        sink = NULL;
        globalArena.tini();
        delete session;
        session = NULL;
    }
}

EDataFileType CDataDocument::open(const char *fileName) {
    FILE *in = fopen(fileName, "rb");
    if (in == NULL)
        return DATA_NOT_A_DATA_FILE;

    EDataFileType type = sniffHeader(in);

    if (type == DATA_NOT_A_DATA_FILE || type == DATA_BAD_VERSION || type == DATA_BAD_WORDSIZE) {
        fclose(in);
        return type;
    }

    // Everything below constructs a reader, which needs the standard declarations table and the memory
    // pool: a standard session (the declarations and the file search, no renderer) and the arena, kept
    // alive for this document's own lifetime (not torn down at the end of this call: the readers/interactive
    // state must survive across ribdata_key()'s re-emit cycle, unlike ribpreview_load's
    // fully-materialized-then-torn-down RIB scene).
    session = new CRiStandardSession();
    globalArena.init();

    // Several reader constructors (e.g. CBrickMap's read path) call info()/error() for
    // legitimate diagnostics, not just on failure. They reach the active context, so one is needed:
    // a plain CRiInterface has no pure virtuals and its default RiError() only forwards to a registered
    // error handler (none here), a safe, silent sink -- exactly what a headless loader needs. It is
    // installed explicitly and removed again with the document.
    sink = new CRiInterface();
    riInstallContext(sink);

    // CRenderer::fromWorld/toWorld/fromWorld1/toWorld1/fromNDC/toNDC and worldBmin/worldBmax
    // are file-scope statics only ever initialized in beginFrame(), which this headless path
    // never reaches -- yet CPhotonMap's and CIrradianceCache's read constructors read them
    // (photonStore.cpp, irradianceCache.cpp). Seed them to identity/+-infinity before
    // constructing any reader, or a photon map/cache silently loads with a degenerate transform.
    identitym(CRenderer::fromWorld);
    identitym(CRenderer::toWorld);
    identitym(CRenderer::fromWorld1);
    identitym(CRenderer::toWorld1);
    identitym(CRenderer::fromNDC);
    identitym(CRenderer::toNDC);
    initv(CRenderer::worldBmin, C_INFINITY, C_INFINITY, C_INFINITY);
    initv(CRenderer::worldBmax, -C_INFINITY, -C_INFINITY, -C_INFINITY);

    matrix from, to;
    identitym(from);
    identitym(to);

    switch (type) {
        case DATA_PHOTONMAP:
            dataView = new CPhotonMap(fileName, in);
            fclose(in); // CPhotonMap neither closes nor retains `in`
            break;

        case DATA_IRRADIANCECACHE:
        case DATA_GATHERCACHE:
            // CIrradianceCache closes `in` itself once the read succeeds.
            dataView = new CIrradianceCacheDocument(fileName, CACHE_READ | CACHE_RDONLY, in, from, to, NULL, CRenderer::worldBmin, CRenderer::worldBmax);
            break;

        case DATA_POINTCLOUD:
            // CPointCloud closes `in` itself on every path.
            dataView = new CPointCloudDocument(fileName, from, to, in);
            break;

        case DATA_BRICKMAP:
            // CBrickMap retains `in` for lazy on-demand brick reads; closes it in its own
            // destructor.
            dataView = new CBrickMapDocument(in, fileName, from, to);
            break;

        case DATA_DEBUGDUMP:
            // The dump format has no header of its own -- CDebugView's FILE* constructor
            // reads its bmin/bmax from byte 0. Retains and closes `in` in its own destructor.
            fseek(in, 0, SEEK_SET);
            dataView = new CDebugView(in, fileName);
            break;

        default:
            fclose(in);
            return DATA_NOT_A_DATA_FILE;
    }

    return type;
}
