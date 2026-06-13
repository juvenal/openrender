// sloMetadata.h — metadata schema for standalone .slo (LLVM bitcode) shader files
//
// .slo files embed all shader metadata as LLVM named metadata nodes so they are
// fully self-contained without a companion .rslo file.
//
// Named metadata layout (all string-valued for portability):
//
//   !openrender.shader.name    = !{!"plastic"}
//   !openrender.shader.type    = !{!"surface"}   ; surface|light|displacement|volume|imager
//   !openrender.shader.version = !{!"1"}
//   !openrender.shader.params  = !{!p0, !p1, ...}
//   !openrender.shader.vars    = !{!v0, ...}     ; local temporaries
//
//   !p0 = !{!"Ka", !"float", !"uniform", !"false", !"1", !"1.0"}
//         ;  name   type      storage     writable   size  default
//
//   !v0 = !{!"_tmp0", !"vector", !"varying", !"false", !"1", !""}
//
// Field order in each parameter/variable node:
//   [0] name      — shader parameter / variable name
//   [1] typeName  — "float"|"vector"|"normal"|"point"|"color"|"matrix"|"string"
//   [2] storage   — "uniform"|"varying"
//   [3] writable  — "true" (output param) or "false"
//   [4] arraySize — "1" for scalar, "N" for array of N
//   [5] default   — default value as a space-separated string; "" for no default
//
// The LLVM entry function name equals the shader name (e.g., function "plastic" for
// plastic.sl). TShaderJitEntry signature: void(int numVertices, void*** stuff, int* tags)
//
#pragma once
#include <string>
#include <vector>

struct SLOParamInfo {
    std::string name;
    std::string typeName;   // "float", "vector", "normal", "point", "color", "matrix", "string"
    std::string storage;    // "uniform" or "varying"
    bool        writable;   // true for output parameters
    int         arraySize;  // 1 = scalar, >1 = array
    std::string defaultStr; // default value as a parseable string, may be empty
};

struct SLOShaderInfo {
    std::string              name;
    std::string              typeName; // "surface", "light", "displacement", "volume", "imager"
    int                      version;
    std::vector<SLOParamInfo> params;  // shader parameters
    std::vector<SLOParamInfo> vars;    // local variables (temporaries)

    bool valid() const { return !name.empty() && !typeName.empty(); }
};
