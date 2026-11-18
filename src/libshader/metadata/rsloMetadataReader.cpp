/**
 * Project: openRender
 *
 * File: rsloMetadataReader.cpp
 *
 * Description:
 *   The .rslo metadata reader (spec 033): wraps the inspection parser's rsloGet()/rsloDelete(), in the same image. The .rslo/.sdr probing and the [INFO] fallback line on stdout are rsloGet's own.
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

// rslo.h's variable types are spelled like common/rendererc.h's EVariableType (the same names, in the same global scope): rename
// them (and the container classes) for the length of the include, and read them as RSLO_TYPE_* and RSLO_CONTAINER_* below
#define TYPE_FLOAT RSLO_TYPE_FLOAT
#define TYPE_VECTOR RSLO_TYPE_VECTOR
#define TYPE_NORMAL RSLO_TYPE_NORMAL
#define TYPE_POINT RSLO_TYPE_POINT
#define TYPE_COLOR RSLO_TYPE_COLOR
#define TYPE_MATRIX RSLO_TYPE_MATRIX
#define TYPE_STRING RSLO_TYPE_STRING
#define CONTAINER_CONSTANT RSLO_CONTAINER_CONSTANT
#define CONTAINER_UNIFORM RSLO_CONTAINER_UNIFORM
#define CONTAINER_VARYING RSLO_CONTAINER_VARYING
#define CONTAINER_VERTEX RSLO_CONTAINER_VERTEX
#include "rslo.h" // the inspection parser: TRSLObjectShader, rsloGet, rsloDelete
#undef TYPE_FLOAT
#undef TYPE_VECTOR
#undef TYPE_NORMAL
#undef TYPE_POINT
#undef TYPE_COLOR
#undef TYPE_MATRIX
#undef TYPE_STRING
#undef CONTAINER_CONSTANT
#undef CONTAINER_UNIFORM
#undef CONTAINER_VARYING
#undef CONTAINER_VERTEX

namespace openrender::shading {

    namespace {

        class CRsloMetadataReader : public IShaderMetadataReader {
            public:
                const char *format() const override { return "rslo"; }

                ShaderMetadata read(const char *nameOrPath, const char *searchPath) const override {
                    ShaderMetadata m;
                    m.format = "rslo";

                    TRSLObjectShader *shader = rsloGet(nameOrPath, searchPath != nullptr ? searchPath : ".");
                    if (shader == nullptr)
                        return m; // NotFound

                    switch (shader->type) {
                        case SHADER_SURFACE:
                            m.typeName = "surface";
                            break;
                        case SHADER_DISPLACEMENT:
                            m.typeName = "displacement";
                            break;
                        case SHADER_VOLUME:
                            m.typeName = "volume";
                            break;
                        case SHADER_LIGHT:
                            m.typeName = "light";
                            break;
                        case SHADER_IMAGER:
                            m.typeName = "imager";
                            break;
                    }
                    m.name = shader->name != nullptr ? shader->name : "";

                    for (TRSLObjectParameter *p = shader->parameters; p; p = p->next) {
                        ShaderParameterMetadata q;
                        q.name = p->name != nullptr ? p->name : "";
                        q.output = p->writable != 0;
                        q.arraySize = p->numItems;
                        q.hasTypedDefault = true;
                        q.space = p->space != nullptr ? p->space : "";

                        switch (p->container) {
                            case RSLO_CONTAINER_CONSTANT:
                                q.storage = "constant";
                                break;
                            case RSLO_CONTAINER_UNIFORM:
                                q.storage = "uniform";
                                break;
                            case RSLO_CONTAINER_VARYING:
                                q.storage = "varying";
                                break;
                            case RSLO_CONTAINER_VERTEX:
                                q.storage = "vertex";
                                break;
                        }
                        switch (p->type) {
                            case RSLO_TYPE_FLOAT:
                                q.type = TYPE_FLOAT;
                                q.typeName = "float";
                                break;
                            case RSLO_TYPE_VECTOR:
                                q.type = TYPE_VECTOR;
                                q.typeName = "vector";
                                break;
                            case RSLO_TYPE_NORMAL:
                                q.type = TYPE_NORMAL;
                                q.typeName = "normal";
                                break;
                            case RSLO_TYPE_POINT:
                                q.type = TYPE_POINT;
                                q.typeName = "point";
                                break;
                            case RSLO_TYPE_COLOR:
                                q.type = TYPE_COLOR;
                                q.typeName = "color";
                                break;
                            case RSLO_TYPE_MATRIX:
                                q.type = TYPE_MATRIX;
                                q.typeName = "matrix";
                                break;
                            case RSLO_TYPE_STRING:
                                q.type = TYPE_STRING;
                                q.typeName = "string";
                                break;
                        }

                        // The defaults, item after item: 1 float, 3 for a vector-like, 16 for a matrix, one string; a missing
                        // vector or matrix is zeros and a missing string is "NULL" (what the listing has always shown)
                        UDefaultVal *cur = &p->defaultValue;
                        if (p->numItems > 1)
                            cur = cur->array;
                        for (int i = 0; i < p->numItems; i++, cur++) {
                            switch (p->type) {
                                case RSLO_TYPE_FLOAT:
                                    q.defaultFloats.push_back(cur->scalar);
                                    break;
                                case RSLO_TYPE_VECTOR:
                                case RSLO_TYPE_NORMAL:
                                case RSLO_TYPE_POINT:
                                case RSLO_TYPE_COLOR:
                                    for (int k = 0; k < 3; k++)
                                        q.defaultFloats.push_back(cur->vector != nullptr ? cur->vector[k] : 0.0f);
                                    break;
                                case RSLO_TYPE_MATRIX:
                                    for (int k = 0; k < 16; k++)
                                        q.defaultFloats.push_back(cur->matrix != nullptr ? cur->matrix[k] : 0.0f);
                                    break;
                                case RSLO_TYPE_STRING:
                                    q.defaultStrings.push_back(cur->string != nullptr ? cur->string : "NULL");
                                    break;
                            }
                        }
                        m.parameters.push_back(std::move(q));
                    }

                    rsloDelete(shader);
                    m.status = EMetadataStatus::Found;
                    return m;
                }
        };

    } // namespace

    const IShaderMetadataReader &rsloMetadataReader() {
        static const CRsloMetadataReader reader;
        return reader;
    }

} // namespace openrender::shading
