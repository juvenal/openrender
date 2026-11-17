/**
 * Project: openRender
 *
 * File: sloMetadataReader.cpp
 *
 * Description:
 *   The .slo metadata reader (spec 033): wraps the bitcode metadata probe of libRSLJitMeta. Compiled only into the shared libRSLInfo, and only with the JIT; the static archive and a JIT-less build answer Unsupported.
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

#include "libshader/shading/llvmJit.h"     // the metadata probe
#include "libshader/shading/sloMetadata.h" // what the probe fills

namespace openrender::shading {

    namespace {

        EVariableType variableTypeOf(const std::string &name) {
            if (name == "vector")
                return TYPE_VECTOR;
            if (name == "normal")
                return TYPE_NORMAL;
            if (name == "point")
                return TYPE_POINT;
            if (name == "color")
                return TYPE_COLOR;
            if (name == "matrix")
                return TYPE_MATRIX;
            if (name == "string")
                return TYPE_STRING;
            return TYPE_FLOAT;
        }

        class CSloMetadataReader : public IShaderMetadataReader {
            public:
                const char *format() const override { return "slo"; }

                ShaderMetadata read(const char *nameOrPath, const char *) const override {
                    ShaderMetadata m;
                    m.format = "slo";

                    SLOShaderInfo info;
                    if (!CLLVMJitEngine::extractMetadataFromFile(nameOrPath, info)) {
                        // The probe answers false for a missing file and for bitcode with no openrender metadata alike
                        FILE *f = fopen(nameOrPath, "rb");
                        if (f != nullptr) {
                            fclose(f);
                            m.status = EMetadataStatus::NoMetadata;
                        }
                        return m; // NotFound otherwise
                    }

                    m.name = info.name;
                    m.typeName = info.typeName;
                    m.formatVersion = std::to_string(info.version);
                    m.usedParameters = (int)info.usedParameters;
                    m.hasJitEntry = info.hasJitEntry;
                    for (const SLOParamInfo &p : info.params) {
                        ShaderParameterMetadata q;
                        q.name = p.name;
                        q.type = variableTypeOf(p.typeName);
                        q.typeName = p.typeName;
                        q.storage = p.storage;
                        q.arraySize = p.arraySize;
                        q.output = p.writable;
                        q.hasTypedDefault = false;
                        q.defaultText = p.defaultStr;
                        m.parameters.push_back(std::move(q));
                    }
                    m.status = EMetadataStatus::Found;
                    return m;
                }
        };

    } // namespace

    const IShaderMetadataReader *sloMetadataReader() {
        static const CSloMetadataReader reader;
        return &reader;
    }

} // namespace openrender::shading
