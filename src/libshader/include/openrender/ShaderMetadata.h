/**
 * Project: openRender
 *
 * File: ShaderMetadata.h
 *
 * Description:
 *   The shader-metadata API (spec 033, FR-016 to FR-018, contracts/shader-metadata.md): what a compiled shader object says about
 *   itself, read from the file without loading or running it. One structure for every format; it names no engine (no bytecode
 *   index, no LLVM type), so the compiler library can publish the same structure at compile time (spec 043). Internal C++: not
 *   installed and not in OPENRENDER_PUBLIC_HEADERS. The library is libRSLInfo (shared, with the .slo reader; static, the .rslo
 *   half only), which is never linked into a program that also links libRSLRuntime: the two parsers share the flex/bison prefix.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef OPENRENDER_SHADERMETADATA_H
#define OPENRENDER_SHADERMETADATA_H

#include <string>
#include <vector>

#include "common/rendererc.h" // EVariableType

namespace openrender::shading {

    enum class EMetadataStatus {
        Found,      // the file was read and the structure is filled
        NotFound,   // no such file (or, for .rslo, no such shader along the search path)
        NoMetadata, // a file of the format that carries no shader metadata (a .slo older than its embedded metadata)
        Unsupported // this library cannot read the format (a .slo in a build without LLVM, or in the static archive)
    };

    struct ShaderParameterMetadata {
            std::string name;
            EVariableType type = TYPE_FLOAT; // common/rendererc.h: engine-free
            std::string typeName;            // "float", "color", ... as the file spells it
            std::string storage;             // "uniform" / "varying" / "constant" / "vertex"
            int arraySize = 1;               // 1 for a scalar
            bool output = false;
            std::string space;                         // a default's space, when the format records one
            std::vector<float> defaultFloats;          // typed defaults, when the format records them, item after item
            std::vector<std::string> defaultStrings;   // string defaults, one per item ("NULL" for an item with no string)
            bool hasTypedDefault = false;              // .rslo: true; .slo: false
            std::string defaultText;                   // .slo: the stored text; .rslo: empty
    };

    struct ShaderMetadata {
            EMetadataStatus status = EMetadataStatus::NotFound;
            std::string format;     // "rslo", "slo"
            std::string typeName;   // "surface", "light", ...
            std::string name;
            std::string formatVersion, languageVersion; // empty when the file does not record them
            int usedParameters = -1;                    // -1 when the format does not record it
            bool hasJitEntry = false;                   // .slo only
            std::vector<ShaderParameterMetadata> parameters;
            std::vector<ShaderParameterMetadata> locals; // none in this spec (sloinfo prints none)
    };

    // Describe a shader object without loading or running it. `format` is "rslo", "slo" or nullptr (by magic: LLVM bitcode -> slo,
    // anything else -> rslo). For rslo, `nameOrPath` may be a bare name searched along `searchPath` (colon-separated; may be null).
    ShaderMetadata describeShaderFile(const char *nameOrPath, const char *format, const char *searchPath);

    // Per-format readers (one per backend; spec 049 adds .oso)
    class IShaderMetadataReader {
        public:
            virtual ~IShaderMetadataReader() = default;
            virtual const char *format() const = 0;
            virtual ShaderMetadata read(const char *nameOrPath, const char *searchPath) const = 0;
    };

} // namespace openrender::shading

#endif
