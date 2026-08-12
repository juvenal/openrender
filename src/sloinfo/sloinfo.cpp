/**
 * Project: openRender
 *
 * File: sloinfo.cpp  (also installed as rsloinfo via symlink)
 *
 * Description:
 *   Unified shader metadata inspector for .rslo (interpreted bytecode) and
 *   .slo (LLVM JIT bitcode) shader files.
 *
 *   Invocation:
 *     sloinfo <file>       — auto-detects format from file magic / extension
 *     rsloinfo <file>      — forces .rslo mode (argv[0] detection)
 *     sloinfo --rslo <file> — explicit .rslo mode
 *     sloinfo --slo  <file> — explicit .slo mode (requires OPENRENDER_HAVE_LLVM)
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "common/global.h"
#include "common/os.h"
#include "rslo.h"   // libshader_runtime: TRSLObjectShader, rsloGet, rsloDelete
#include "logging.hpp"

#ifdef OPENRENDER_HAVE_LLVM
#include "libshader/shading/llvmJit.h"    // CLLVMJitEngine::extractMetadataFromFile
#include "libshader/shading/sloMetadata.h" // SLOShaderInfo
#endif

// -------------------------------------------------------------------------
// LLVM bitcode magic detection.
// Raw LLVM bitcode starts with 'B','C' (0x42,0x43).
// Wrapped bitcode starts with 0xDE,0xC0,0x17,0x0B.
// -------------------------------------------------------------------------
static bool isLLVMBitcode(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    unsigned char hdr[4] = {0, 0, 0, 0};
    (void)fread(hdr, 1, 4, f);
    fclose(f);
    // Raw bitcode
    if (hdr[0] == 0x42 && hdr[1] == 0x43) return true;
    // Wrapper format
    if (hdr[0] == 0xDE && hdr[1] == 0xC0 && hdr[2] == 0x17 && hdr[3] == 0x0B) return true;
    return false;
}

// -------------------------------------------------------------------------
// Display .rslo metadata using the existing libshader_runtime reader.
// -------------------------------------------------------------------------
static int displayRslo(const char *arg) {
    char tmp[1024];
    tmp[0] = '\0';

    const char *openrenderHome = osEnvironment("OPENRENDERHOME");
    const char *shaders        = osEnvironment("SHADERS");

    strcpy(tmp, ".");
    if (openrenderHome) {
        strcat(tmp, ":"); strcat(tmp, openrenderHome); strcat(tmp, "/shaders");
    }
    if (shaders) {
        strcat(tmp, ":"); strcat(tmp, shaders);
    }
    osFixSlashes(tmp);

    TRSLObjectShader *cShader = rsloGet(arg, tmp);
    if (!cShader) {
        fprintf(stderr, "Failed to find shader \"%s\"\n", arg);
        return 1;
    }

    switch (cShader->type) {
    case SHADER_SURFACE:      fprintf(stdout, "surface ");      break;
    case SHADER_DISPLACEMENT: fprintf(stdout, "displacement "); break;
    case SHADER_VOLUME:       fprintf(stdout, "volume ");       break;
    case SHADER_LIGHT:        fprintf(stdout, "light ");        break;
    case SHADER_IMAGER:       fprintf(stdout, "imager ");       break;
    }
    fprintf(stdout, "\"%s\"\n", cShader->name);

    for (TRSLObjectParameter *p = cShader->parameters; p; p = p->next) {
        fprintf(stdout, "\t\"%s\" ", p->name);

        if (p->writable)
            fprintf(stdout, "\"output ");
        else
            fprintf(stdout, "\"");

        switch (p->container) {
        case CONTAINER_CONSTANT: fprintf(stdout, "constant "); break;
        case CONTAINER_UNIFORM:  fprintf(stdout, "uniform ");  break;
        case CONTAINER_VARYING:  fprintf(stdout, "varying ");  break;
        case CONTAINER_VERTEX:   fprintf(stdout, "vertex ");   break;
        }

        switch (p->type) {
        case TYPE_FLOAT:  fprintf(stdout, "float");  break;
        case TYPE_VECTOR: fprintf(stdout, "vector"); break;
        case TYPE_NORMAL: fprintf(stdout, "normal"); break;
        case TYPE_POINT:  fprintf(stdout, "point");  break;
        case TYPE_COLOR:  fprintf(stdout, "color");  break;
        case TYPE_MATRIX: fprintf(stdout, "matrix"); break;
        case TYPE_STRING: fprintf(stdout, "string"); break;
        }

        if (p->numItems > 1)
            fprintf(stdout, "[%d]", p->numItems);
        fprintf(stdout, "\"\n");

        fprintf(stdout, "\t\tDefault value: ");
        UDefaultVal *cur = &p->defaultValue;
        if (p->numItems > 1) cur = cur->array;

        for (int i = 0; i < p->numItems; i++, cur++) {
            switch (p->type) {
            case TYPE_FLOAT:
                fprintf(stdout, "%g ", cur->scalar);
                break;
            case TYPE_VECTOR: case TYPE_NORMAL: case TYPE_POINT: case TYPE_COLOR:
                if (p->space) fprintf(stdout, "\"%s\" ", p->space);
                if (cur->vector)
                    fprintf(stdout, "[%g %g %g] ",
                            cur->vector[0], cur->vector[1], cur->vector[2]);
                else
                    fprintf(stdout, "[0 0 0] ");
                break;
            case TYPE_MATRIX:
                if (cur->matrix) {
                    fprintf(stdout, "[");
                    for (int k = 0; k < 16; k++)
                        fprintf(stdout, "%g%s", cur->matrix[k], k < 15 ? " " : "");
                    fprintf(stdout, "] ");
                } else {
                    fprintf(stdout, "[0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0] ");
                }
                break;
            case TYPE_STRING:
                fprintf(stdout, "\"%s\" ", cur->string ? cur->string : "NULL");
                break;
            }
        }
        fprintf(stdout, "\n");
    }

    rsloDelete(cShader);
    return 0;
}

// -------------------------------------------------------------------------
// Display .slo metadata using the LLVM named metadata reader.
// -------------------------------------------------------------------------
#ifdef OPENRENDER_HAVE_LLVM
static int displaySlo(const char *filename) {
    SLOShaderInfo info;
    if (!CLLVMJitEngine::extractMetadataFromFile(filename, info)) {
        fprintf(stderr,
                "sloinfo: '%s' is LLVM bitcode but contains no openrender metadata.\n"
                "  (The file may be a pre-Phase-C4 .slo without embedded metadata.)\n",
                filename);
        return 1;
    }

    if (!info.hasJitEntry) {
        fprintf(stderr,
                "sloinfo: warning: '%s' has openrender metadata but no matching "
                "JIT entry function \"%s\" — not a valid JIT-callable shader.\n",
                filename, info.name.c_str());
    }

    fprintf(stdout, "%s \"%s\"%s\n", info.typeName.c_str(), info.name.c_str(),
            info.hasJitEntry ? " (JIT version)" : "");

    for (const SLOParamInfo &p : info.params) {
        fprintf(stdout, "\t\"%s\" \"", p.name.c_str());
        if (p.writable)   fprintf(stdout, "output ");
        fprintf(stdout, "%s ", p.storage.c_str());
        fprintf(stdout, "%s", p.typeName.c_str());
        if (p.arraySize > 1) fprintf(stdout, "[%d]", p.arraySize);
        fprintf(stdout, "\"\n");
        if (!p.defaultStr.empty())
            fprintf(stdout, "\t\tDefault value: %s\n", p.defaultStr.c_str());
    }

    return 0;
}
#endif // OPENRENDER_HAVE_LLVM

// =========================================================================
// main
// =========================================================================
int main(int argc, char *argv[]) {
    set_log_level(LogLevel::NONE);

    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s [--rslo|--slo] <shader_file>\n"
                "  Auto-detects .rslo (text) vs .slo (LLVM bitcode) from file magic.\n"
                "  Invoking as 'rsloinfo' forces .rslo mode.\n",
                argv[0]);
        return 1;
    }

    // Determine default mode from argv[0]: "rsloinfo" → force .rslo.
    const char *progname = strrchr(argv[0], '/');
    progname = progname ? progname + 1 : argv[0];
    bool forceRslo = (strstr(progname, "rsloinfo") != nullptr);
    bool forceSlo  = false;
    const char *fileArg = argv[1];

    if (argc >= 3) {
        if (strcmp(argv[1], "--rslo") == 0) { forceRslo = true;  fileArg = argv[2]; }
        if (strcmp(argv[1], "--slo")  == 0) { forceSlo  = true;  fileArg = argv[2]; }
    }

    // Auto-detect format when no flag is given.
    bool isBitcode = isLLVMBitcode(fileArg);

    if (forceRslo || (!forceSlo && !isBitcode)) {
        return displayRslo(fileArg);
    }

#ifdef OPENRENDER_HAVE_LLVM
    return displaySlo(fileArg);
#else
    fprintf(stderr,
            "sloinfo: '%s' appears to be LLVM bitcode, but this build was compiled\n"
            "without LLVM support.  Rebuild with OPENRENDER_HAVE_LLVM to inspect .slo files.\n",
            fileArg);
    return 1;
#endif
}
