/**
 * Project: openRender
 *
 * File: serviceTypes.h
 *
 * Description:
 *   The vocabulary of the renderer services, version 2 (spec 026, contracts/renderer-services-v2.md):
 *   opaque handles, query status and type, and the request and answer structs of the three queries
 *   Shading asks the renderer: getAttribute(), getMessage() and getMatrix(). Shading owns this
 *   header; it includes only Infrastructure headers (FR-003) and names no Geometry, Hiders or
 *   renderer-core type.
 *
 *   Validity: a handle, a request and an answer are valid for the duration of the shading call that
 *   made them. An implementer never stores them.
 *
 *   Version: kRendererServicesVersion (rendererServices.h). Adding an enumerator or appending a
 *   field is MINOR; removing or renaming one is MAJOR.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SERVICETYPES_H
#define SERVICETYPES_H

#include <cstdint>

#include "common/global.h"
#include "common/rendererc.h" // ECoordinateSystem, EVariableType
#include "common/variable.h"  // CVariable

namespace openrender::shading {

    ///////////////////////////////////////////////////////////////////////
    // Handles
    ///////////////////////////////////////////////////////////////////////

    // The shaded object, opaque. Made only by CShadingContext::objectHandle(). Null when there is no
    // object (the imager, a bare execution). Valid for the duration of the shading call that made it;
    // never stored by an implementer. Only an implementer casts it back to its own type: the renderer's
    // handle points at a const CObject (the shaded object), and a double chooses its own meaning.
    struct ObjectHandle {
            const void *ptr = nullptr;
            explicit operator bool() const { return ptr != nullptr; }
    };

    // A shader instance (the executing shader, or a light), opaque. Made from a CShaderInstance *.
    // Valid for the duration of the shading call that made it; never stored by an implementer.
    struct ShaderHandle {
            const void *ptr = nullptr;
            explicit operator bool() const { return ptr != nullptr; }
    };

    ///////////////////////////////////////////////////////////////////////
    // Query vocabulary
    ///////////////////////////////////////////////////////////////////////

    // Found: the answer is valid. NotFound: no such name. TypeMismatch: the stored value cannot be copied
    // safely into the type the shader expects (see AttributeValue). NoObject: an object space was requested
    // with a null object (getMatrix() only). Unavailable: the implementer does not provide the service (the
    // default body of the interface member).
    enum class EQueryStatus : uint8_t { Found, NotFound, TypeMismatch, NoObject, Unavailable };

    // What the shader expects: from the interpreter's DEFFUNC overload (F, V, M, S) or the JIT's
    // resultKind (1, 3, 16, -1).
    enum class QueryType : uint8_t { Float, Triple, Matrix, String };

    // Which name table getAttribute() searches.
    enum class AttributeScope : uint8_t { Attribute, Option, RendererInfo };

    // Where getMessage() reads a shader parameter. incident() maps to Interior and opposite() to Exterior,
    // as the starting tree does.
    enum class MessageSource : uint8_t { Surface, Displacement, Atmosphere, Interior, Exterior, Light };

    ///////////////////////////////////////////////////////////////////////
    // getAttribute(): attribute(), option(), rendererinfo()
    ///////////////////////////////////////////////////////////////////////

    struct AttributeRequest {
            AttributeScope scope;
            ObjectHandle object;  // ignored for Option and RendererInfo
            const char *name;     // name[0] of the call: one request per call, not per lane
            QueryType expected;
    };

    // Caller-owned: Shading passes one in, and the implementer fills it. `data` may point into this object's
    // own inlineValue (a computed answer), so the answer is never returned by value: a copy would leave `data`
    // pointing into the destroyed original.
    //
    // When status is Found, data is non-null and count >= 1. TypeMismatch (FR-026) means the copy cannot be
    // made safely: Triple against more than 3 floats, Matrix against more than 16, String against a non-string,
    // any other type against a string. Shading refuses it like NotFound (the destination is untouched and the
    // builtin returns 0) and warns once; `type` and `count` then describe the stored value for the diagnostic.
    struct AttributeValue {
            EQueryStatus status = EQueryStatus::Unavailable;
            EVariableType type = TYPE_FLOAT;
            int count = 0;              // floats, or char * slots when type == TYPE_STRING
            const void *data = nullptr; // the renderer's storage (valid until the end of the shading call) or inlineValue
            float inlineValue[16] = {}; // storage for computed answers (Sides, Format, CropWindow, ...)

            AttributeValue() = default;
            AttributeValue(const AttributeValue &) = delete; // see above
            AttributeValue &operator=(const AttributeValue &) = delete;
    };

    ///////////////////////////////////////////////////////////////////////
    // getMessage(): surface(), displacement(), atmosphere(), incident(), opposite(), lightsource()
    ///////////////////////////////////////////////////////////////////////

    struct MessageRequest {
            MessageSource source;
            ObjectHandle object; // for the five object sources
            ShaderHandle light;  // for Light
            const char *name;    // the parameter name
            QueryType expected;
            void *dest;          // where a default value is written, in the parameter's own layout
            // true where the caller reads the producer's current value (surface, displacement, atmosphere, the
            // interpreter's lightsource). When true, the answer returns the producer's CVariable for every
            // parameter except an immutable parameter of a light (shader.cpp getParameter), and writes nothing
            // into `dest` for it; otherwise (or when false) `dest` receives the default value.
            bool wantVariable;
    };

    struct MessageAnswer {
            EQueryStatus status = EQueryStatus::Unavailable;
            // Set (only when the request had wantVariable) when the parameter's current value is to be read
            // from the producer's storage instead of `dest`: Shading reads variable->storage, ->entry,
            // ->numFloats and ->container. Null when the default was written into `dest`. Owned by the
            // producer's shader; valid until the end of the shading call.
            CVariable *variable = nullptr;
            // Meaningful only with a variable and for the Light source: the parameter's index in the light's
            // saved state; -1 otherwise.
            int globalIndex = -1;
    };

    ///////////////////////////////////////////////////////////////////////
    // getMatrix(): every coordinate-system lookup
    ///////////////////////////////////////////////////////////////////////

    struct MatrixRequest {
            const char *name;    // a system or colour-space name, including "object", "shader", "light", "current", "camera", "world"
            ObjectHandle object; // for "object"
            ShaderHandle shader; // the executing instance, for "shader"
            ShaderHandle light;  // the executing light, for "light"
            float time;          // normalised shutter time; Shading passes 0 (no call site carries one) and the renderer ignores it
    };

    struct MatrixAnswer {
            EQueryStatus status = EQueryStatus::Unavailable; // Found, NotFound, NoObject or Unavailable
            // What the name is; callers branch on it. Legal values: COORDINATE_OBJECT, CAMERA, WORLD, SHADER,
            // LIGHT, NDC, RASTER, SCREEN, CURRENT, CUSTOM, and COLOR_RGB, HSL, HSV, XYZ, CIE, YIQ, XYY. Any
            // other value is treated as an unknown system (identity matrix, one warning). For NoObject the
            // kind is COORDINATE_OBJECT.
            ECoordinateSystem kind = COORDINATE_CUSTOM;
            // Column-major 4x4: `from` takes points of the named system into the current (camera) system, `to`
            // is its inverse. Valid until the end of the shading call; never null when Found.
            const float *from = nullptr;
            const float *to = nullptr;
    };

} // namespace openrender::shading

#endif
