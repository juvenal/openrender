/**
 * Project: openRender
 *
 * File: shaderMetadata.cpp
 *
 * Description:
 *   describeShaderFile(): picks the reader of a format (spec 033, FR-016); the readers are in rsloMetadataReader.cpp and sloMetadataReader.cpp.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "libshader/include/openrender/ShaderMetadata.h"

#include <stdio.h>

namespace openrender::shading {

    const IShaderMetadataReader &rsloMetadataReader(); // rsloMetadataReader.cpp
#ifdef RSLINFO_WITH_SLO
    const IShaderMetadataReader *sloMetadataReader(); // sloMetadataReader.cpp: the shared library with the JIT only
#else
    // The static archive and a JIT-less build have no .slo reader (no LLVM): a .slo request answers Unsupported
    static const IShaderMetadataReader *sloMetadataReader() { return nullptr; }
#endif

    namespace {

        // Raw LLVM bitcode starts with 'B','C' (0x42,0x43); wrapped bitcode with 0xDE,0xC0,0x17,0x0B
        bool isLLVMBitcode(const char *filename) {
            FILE *f = fopen(filename, "rb");
            if (!f)
                return false;
            unsigned char hdr[4] = {0, 0, 0, 0};
            const size_t magicRead = fread(hdr, 1, sizeof(hdr), f);
            fclose(f);
            if (magicRead != sizeof(hdr))
                return false;
            return (hdr[0] == 0x42 && hdr[1] == 0x43) || (hdr[0] == 0xDE && hdr[1] == 0xC0 && hdr[2] == 0x17 && hdr[3] == 0x0B);
        }

    } // namespace

    ShaderMetadata describeShaderFile(const char *nameOrPath, const char *format, const char *searchPath) {
        ShaderMetadata none;
        if (nameOrPath == nullptr)
            return none;

        const bool slo = (format != nullptr) ? std::string(format) == "slo" : isLLVMBitcode(nameOrPath);
        if (format != nullptr && std::string(format) != "slo" && std::string(format) != "rslo") {
            none.status = EMetadataStatus::Unsupported;
            none.format = format;
            return none;
        }

        if (!slo)
            return rsloMetadataReader().read(nameOrPath, searchPath);

        const IShaderMetadataReader *reader = sloMetadataReader();
        if (reader == nullptr) {
            none.status = EMetadataStatus::Unsupported;
            none.format = "slo";
            return none;
        }
        return reader->read(nameOrPath, searchPath);
    }

} // namespace openrender::shading
