#include "common/global.h"
#include "ribpreview_api.h"
#include "ri/dataLoad.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

// JSON has no token for inf/NaN. A degenerate/empty scene (e.g. a RIB with no geometry and no
// explicit camera) can otherwise leak a non-finite value from previewContext.cpp's clipping-
// plane synthesis straight into this output -- guard at the point of emission rather than
// upstream, since that synthesis is pre-existing spec-006 territory this feature doesn't own.
static double sane(double v) {
    return std::isfinite(v) ? v : 0.0;
}

static const char *USAGE =
    "Usage: orender-wire [OPTIONS] <file>\n"
    "\n"
    "  <file>                 RIB scene, or an openRender data-structure file (photon map,\n"
    "                         irradiance/gather cache, point cloud, brick map, or a raw\n"
    "                         debug-geometry dump). Type is auto-detected from content.\n"
    "\n"
    "  --json                 Headless mode: write a JSON description to stdout and exit.\n"
    "  --type=auto|rib|data   Override auto-detection. Default: auto.\n"
    "  --version              Print \"orender-wire <version>\" and exit 0.\n"
    "  -h, --help             Print usage and exit 0.\n"
    "  --                     End of options.\n";

static void printJsonString(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s != '\0'; s++) {
        switch (*s) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if ((unsigned char)*s < 0x20)
                    fprintf(f, "\\u%04x", *s);
                else
                    fputc(*s, f);
        }
    }
    fputc('"', f);
}

static const char *documentTypeName(RibDataType t) {
    switch (t) {
        case RIBDATA_TYPE_PHOTONMAP: return "photonmap";
        case RIBDATA_TYPE_IRRADIANCECACHE: return "irradiancecache";
        case RIBDATA_TYPE_GATHERCACHE: return "gathercache";
        case RIBDATA_TYPE_POINTCLOUD: return "pointcloud";
        case RIBDATA_TYPE_BRICKMAP: return "brickmap";
        default: return "debugdump";
    }
}

static const char *drawModeName(RibDataType t, int mode) {
    if (t == RIBDATA_TYPE_BRICKMAP) {
        switch (mode) {
            case 0: return "boxes";
            case 1: return "discs";
            default: return "points";
        }
    }
    if (t == RIBDATA_TYPE_POINTCLOUD)
        return (mode == 1) ? "discs" : "points";
    return "fixed"; // photon map / irradiance / gather cache / debug dump: no user control
}

static bool fileExists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Version/major.minor.patch from src/ri/dataLoad.h's EDataFileType, mapped to a fixed-width
// array for the "fileVersion" JSON field. openRender's own project version is unrelated to a
// data file's on-disk format version, which dataSniff()/CDataDocument don't currently surface
// per-field -- report the tool's own version here, consistent with "fileVersion" describing
// what *this build* understands, since CDataDocument::open() already validated compatibility
// (a mismatched file never reaches this point, see exit code 4).
static void writeCommonEnvelopeOpen(FILE *out, const char *path, const char *documentType) {
    fprintf(out, "{\n");
    fprintf(out, "  \"schemaVersion\": 1,\n");
    fprintf(out, "  \"tool\": \"orender-wire\",\n");
    fprintf(out, "  \"toolVersion\": ");
    printJsonString(out, openrender_version_string());
    fprintf(out, ",\n  \"file\": ");
    printJsonString(out, path);
    fprintf(out, ",\n  \"documentType\": ");
    printJsonString(out, documentType);
    fprintf(out, ",\n");
}

static int emitRibJson(const char *path) {
    PreviewSceneC *scene = ribpreview_load(path);
    if (scene == NULL)
        return 3;

    writeCommonEnvelopeOpen(stdout, path, "rib");
    fprintf(stdout, "  \"bounds\": { \"min\": [%g, %g, %g], \"max\": [%g, %g, %g] },\n",
            scene->bounds.sceneBoundsMin[0], scene->bounds.sceneBoundsMin[1], scene->bounds.sceneBoundsMin[2],
            scene->bounds.sceneBoundsMax[0], scene->bounds.sceneBoundsMax[1], scene->bounds.sceneBoundsMax[2]);
    fprintf(stdout, "  \"warnings\": [],\n");
    fprintf(stdout, "  \"scene\": { \"lineVertexCount\": %d, \"lineSegmentCount\": %d },\n",
            scene->vertexCount, scene->vertexCount / 2);
    fprintf(stdout, "  \"camera\": {\n");
    fprintf(stdout, "    \"projectionType\": \"%s\",\n", scene->camera.projectionType == 0 ? "perspective" : "orthographic");
    fprintf(stdout, "    \"fov\": %g,\n", sane(scene->camera.fov));
    fprintf(stdout, "    \"frameAspectRatio\": %g,\n", sane(scene->camera.frameAspectRatio));
    fprintf(stdout, "    \"nearPlane\": %g,\n", sane(scene->camera.nearPlane));
    fprintf(stdout, "    \"farPlane\": %g\n", sane(scene->camera.farPlane));
    fprintf(stdout, "  }\n}\n");

    ribpreview_free(scene);
    return 0;
}

static int emitDataJson(const char *path) {
    int err = 0;
    RibDataDocument *doc = ribdata_open(path, &err);
    if (doc == NULL) {
        fprintf(stderr, "orender-wire: \"%s\" is not a recognized data file\n", path);
        return 4;
    }

    const DataSceneC *snap = ribdata_snapshot(doc);
    RibDataType type = snap->documentType;

    writeCommonEnvelopeOpen(stdout, path, documentTypeName(type));
    fprintf(stdout, "  \"bounds\": { \"min\": [%g, %g, %g], \"max\": [%g, %g, %g] },\n",
            snap->bounds.sceneBoundsMin[0], snap->bounds.sceneBoundsMin[1], snap->bounds.sceneBoundsMin[2],
            snap->bounds.sceneBoundsMax[0], snap->bounds.sceneBoundsMax[1], snap->bounds.sceneBoundsMax[2]);

    fprintf(stdout, "  \"warnings\": [");
    if (snap->decimatedCount > 0)
        fprintf(stdout, "\"%d primitives dropped by the display cap\"", snap->decimatedCount);
    fprintf(stdout, "],\n");

    int triCount = snap->triangles.count / 3;
    int lineCount = snap->lines.count / 2;
    fprintf(stdout, "  \"data\": {\n");
    fprintf(stdout, "    \"fileVersion\": [%d, %d, %d],\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    fprintf(stdout, "    \"primitives\": { \"lines\": %d, \"points\": %d, \"triangles\": %d, \"disks\": %d },\n",
            lineCount, snap->points.count, triCount, snap->sourceDiskCount);
    fprintf(stdout, "    \"decimated\": %d,\n", snap->decimatedCount);
    fprintf(stdout, "    \"channels\": [");
    for (int i = 0; i < snap->numChannels; i++) {
        if (i > 0)
            fprintf(stdout, ", ");
        const char *name = ribdata_channel_name(doc, i);
        printJsonString(stdout, name != NULL ? name : "");
    }
    fprintf(stdout, "],\n");
    fprintf(stdout, "    \"currentChannel\": %d,\n", snap->currentChannel);
    fprintf(stdout, "    \"detailLevel\": %d,\n", snap->detailLevel);
    fprintf(stdout, "    \"drawMode\": \"%s\"\n", drawModeName(type, snap->drawMode));
    fprintf(stdout, "  },\n");

    fprintf(stdout, "  \"camera\": {\n");
    fprintf(stdout, "    \"projectionType\": \"%s\",\n", snap->camera.projectionType == 0 ? "perspective" : "orthographic");
    fprintf(stdout, "    \"fov\": %g,\n", sane(snap->camera.fov));
    fprintf(stdout, "    \"frameAspectRatio\": %g,\n", sane(snap->camera.frameAspectRatio));
    fprintf(stdout, "    \"nearPlane\": %g,\n", sane(snap->camera.nearPlane));
    fprintf(stdout, "    \"farPlane\": %g\n", sane(snap->camera.farPlane));
    fprintf(stdout, "  }\n}\n");

    ribdata_close(doc);
    return 0;
}

WireCliAction wireCliRun(int argc, char **argv, char **outPath, int *exitCode) {
    bool jsonMode = false;
    const char *typeOverride = "auto";
    const char *file = NULL;
    bool endOfOptions = false;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!endOfOptions && strcmp(a, "--") == 0) {
            endOfOptions = true;
        } else if (!endOfOptions && (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0)) {
            fputs(USAGE, stdout);
            *exitCode = 0;
            return WIRE_CLI_EXIT;
        } else if (!endOfOptions && strcmp(a, "--version") == 0) {
            printf("orender-wire %s\n", openrender_version_string());
            *exitCode = 0;
            return WIRE_CLI_EXIT;
        } else if (!endOfOptions && strcmp(a, "--json") == 0) {
            jsonMode = true;
        } else if (!endOfOptions && strncmp(a, "--type=", 7) == 0) {
            typeOverride = a + 7;
            if (strcmp(typeOverride, "auto") != 0 && strcmp(typeOverride, "rib") != 0 && strcmp(typeOverride, "data") != 0) {
                fputs(USAGE, stderr);
                *exitCode = 1;
                return WIRE_CLI_EXIT;
            }
        } else if (!endOfOptions && a[0] == '-' && a[1] != '\0') {
            fputs(USAGE, stderr);
            *exitCode = 1;
            return WIRE_CLI_EXIT;
        } else {
            if (file != NULL) {
                fputs(USAGE, stderr);
                *exitCode = 1;
                return WIRE_CLI_EXIT;
            }
            file = a;
        }
    }

    if (file == NULL) {
        fputs(USAGE, stderr);
        *exitCode = 1;
        return WIRE_CLI_EXIT;
    }

    if (!fileExists(file)) {
        fprintf(stderr, "orender-wire: \"%s\": No such file or directory\n", file);
        *exitCode = 2;
        return WIRE_CLI_EXIT;
    }

    if (!jsonMode) {
        *outPath = strdup(file);
        return WIRE_CLI_OPEN;
    }

    bool isData;
    if (strcmp(typeOverride, "rib") == 0) {
        isData = false;
    } else if (strcmp(typeOverride, "data") == 0) {
        isData = true;
    } else {
        // -1 == no data-file magic at all (try RIB); -2 == magic matched but the file is
        // incompatible (bad version/word-size) -- that is still a data file, and routing it to
        // the RIB path would silently "succeed" with an empty scene instead of exit code 4.
        isData = ribdata_sniff(file) != -1;
    }

    *exitCode = isData ? emitDataJson(file) : emitRibJson(file);
    return WIRE_CLI_EXIT;
}
