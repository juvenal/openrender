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
 *     sloinfo --slo  <file> — explicit .slo mode (needs a libRSLInfo built with LLVM)
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
#include "logging.hpp"
#include "libshader/include/openrender/ShaderMetadata.h" // the shader-metadata API (libRSLInfo)

using namespace openrender::shading;

// -------------------------------------------------------------------------
// LLVM bitcode magic detection.
// Raw LLVM bitcode starts with 'B','C' (0x42,0x43).
// Wrapped bitcode starts with 0xDE,0xC0,0x17,0x0B.
// -------------------------------------------------------------------------
static bool isLLVMBitcode(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f)
        return false;
    unsigned char hdr[4] = {0, 0, 0, 0};
    // The count is checked rather than discarded: a short read is not an error
    // as far as fread is concerned, and a (void) cast does not suppress glibc's
    // warn_unused_result on it. A file shorter than the 4-byte magic cannot be
    // a shader object under either encoding tested below.
    const size_t magicRead = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    if (magicRead != sizeof(hdr))
        return false;
    // Raw bitcode
    if (hdr[0] == 0x42 && hdr[1] == 0x43)
        return true;
    // Wrapper format
    if (hdr[0] == 0xDE && hdr[1] == 0xC0 && hdr[2] == 0x17 && hdr[3] == 0x0B)
        return true;
    return false;
}

// -------------------------------------------------------------------------
// Display .rslo metadata, read through the metadata API.
// -------------------------------------------------------------------------
static int displayRslo(const char *arg) {
    char tmp[1024];
    tmp[0] = '\0';

    const char *openrenderHome = osEnvironment("OPENRENDERHOME");
    const char *shaders = osEnvironment("SHADERS");

    strcpy(tmp, ".");
    if (openrenderHome) {
        strcat(tmp, ":");
        strcat(tmp, openrenderHome);
        strcat(tmp, "/shaders");
    }
    if (shaders) {
        strcat(tmp, ":");
        strcat(tmp, shaders);
    }
    osFixSlashes(tmp);

    const ShaderMetadata m = describeShaderFile(arg, "rslo", tmp);
    if (m.status != EMetadataStatus::Found) {
        fprintf(stderr, "Failed to find shader \"%s\"\n", arg);
        return 1;
    }

    // The shader's type, then its name
    if (!m.typeName.empty())
        fprintf(stdout, "%s ", m.typeName.c_str());
    fprintf(stdout, "\"%s\"\n", m.name.c_str());

    for (const ShaderParameterMetadata &p : m.parameters) {
        fprintf(stdout, "\t\"%s\" ", p.name.c_str());

        if (p.output)
            fprintf(stdout, "\"output ");
        else
            fprintf(stdout, "\"");

        fprintf(stdout, "%s ", p.storage.c_str());
        fprintf(stdout, "%s", p.typeName.c_str());

        if (p.arraySize > 1)
            fprintf(stdout, "[%d]", p.arraySize);
        fprintf(stdout, "\"\n");

        fprintf(stdout, "\t\tDefault value: ");
        size_t floatAt = 0, stringAt = 0;
        for (int i = 0; i < p.arraySize; i++) {
            switch (p.type) {
                case TYPE_FLOAT:
                    fprintf(stdout, "%g ", p.defaultFloats[floatAt++]);
                    break;
                case TYPE_VECTOR:
                case TYPE_NORMAL:
                case TYPE_POINT:
                case TYPE_COLOR:
                    if (!p.space.empty())
                        fprintf(stdout, "\"%s\" ", p.space.c_str());
                    fprintf(stdout, "[%g %g %g] ", p.defaultFloats[floatAt], p.defaultFloats[floatAt + 1], p.defaultFloats[floatAt + 2]);
                    floatAt += 3;
                    break;
                case TYPE_MATRIX:
                    fprintf(stdout, "[");
                    for (int k = 0; k < 16; k++)
                        fprintf(stdout, "%g%s", p.defaultFloats[floatAt + k], k < 15 ? " " : "");
                    fprintf(stdout, "] ");
                    floatAt += 16;
                    break;
                case TYPE_STRING:
                    fprintf(stdout, "\"%s\" ", p.defaultStrings[stringAt++].c_str());
                    break;
                default: // the other variable types are not shader parameters
                    break;
            }
        }
        fprintf(stdout, "\n");
    }

    return 0;
}

// -------------------------------------------------------------------------
// Display .slo metadata, read through the metadata API.
// -------------------------------------------------------------------------
static int displaySlo(const char *filename) {
    const ShaderMetadata m = describeShaderFile(filename, "slo", nullptr);

    if (m.status == EMetadataStatus::Unsupported) {
        // The library has no .slo reader: a build without LLVM
        fprintf(stderr,
                "sloinfo: '%s' appears to be LLVM bitcode, but this build was compiled\n"
                "without LLVM support.  Rebuild with OPENRENDER_HAVE_LLVM to inspect .slo files.\n",
                filename);
        return 1;
    }
    if (m.status != EMetadataStatus::Found) {
        fprintf(stderr,
                "sloinfo: '%s' is LLVM bitcode but contains no openrender metadata.\n"
                "  (The file may be a pre-Phase-C4 .slo without embedded metadata.)\n",
                filename);
        return 1;
    }

    if (!m.hasJitEntry) {
        fprintf(stderr,
                "sloinfo: warning: '%s' has openrender metadata but no matching "
                "JIT entry function \"%s\" — not a valid JIT-callable shader.\n",
                filename, m.name.c_str());
    }

    fprintf(stdout, "%s \"%s\"%s\n", m.typeName.c_str(), m.name.c_str(), m.hasJitEntry ? " (JIT version)" : "");

    for (const ShaderParameterMetadata &p : m.parameters) {
        fprintf(stdout, "\t\"%s\" \"", p.name.c_str());
        if (p.output)
            fprintf(stdout, "output ");
        fprintf(stdout, "%s ", p.storage.c_str());
        fprintf(stdout, "%s", p.typeName.c_str());
        if (p.arraySize > 1)
            fprintf(stdout, "[%d]", p.arraySize);
        fprintf(stdout, "\"\n");
        if (!p.defaultText.empty())
            fprintf(stdout, "\t\tDefault value: %s\n", p.defaultText.c_str());
    }

    return 0;
}

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
    bool forceSlo = false;
    const char *fileArg = argv[1];

    if (argc >= 3) {
        if (strcmp(argv[1], "--rslo") == 0) {
            forceRslo = true;
            fileArg = argv[2];
        }
        if (strcmp(argv[1], "--slo") == 0) {
            forceSlo = true;
            fileArg = argv[2];
        }
    }

    // Auto-detect format when no flag is given.
    bool isBitcode = isLLVMBitcode(fileArg);

    if (forceRslo || (!forceSlo && !isBitcode)) {
        return displayRslo(fileArg);
    }

    return displaySlo(fileArg);
}
