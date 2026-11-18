# Contract: the shader-metadata API and `sloinfo`

Header: `src/libshader/include/openrender/ShaderMetadata.h` (internal C++; not installed; not in
`OPENRENDER_PUBLIC_HEADERS`). Library: **`libRSLInfo`** (shared plus static; `src/libshader/metadata/` for the API and readers, `src/libshader/runtime/` for the inspection parser it contains; Shading). `libRSLInfo` is the maintainer's name (2026-11-14) for the planned `libshader_metadata` **and** for `libshader_runtime`, which it absorbs. Implements
FR-016, FR-017, FR-018. Decision: research.md R15 (**approved, P-Q7**).

## API

```cpp
namespace openrender::shading {

    enum class EMetadataStatus { Found, NotFound, NoMetadata, Unsupported };

    struct ShaderParameterMetadata {
        std::string name;
        EVariableType type;          // common/variable.h (Infra), engine-free
        std::string typeName;        // "float", "color", ... as the file spells it
        std::string storage;         // "uniform" / "varying" / "constant" / "vertex"
        int arraySize;               // 1 for a scalar
        bool output;
        std::string space;           // a default's space, when the format records one
        std::vector<float> defaultFloats;          // typed defaults, when the format records them
        std::vector<std::string> defaultStrings;
        bool hasTypedDefault;        // .rslo: true (UDefaultVal); .slo: false
        std::string defaultText;     // .slo: the stored text; .rslo: empty
    };

    struct ShaderMetadata {
        EMetadataStatus status = EMetadataStatus::NotFound;
        std::string format;          // "rslo", "slo"
        std::string typeName;        // "surface", "light", ...
        std::string name;
        std::string formatVersion, languageVersion;   // empty when the file does not record them
        int usedParameters = -1;     // -1 when the format does not record it
        bool hasJitEntry = false;    // .slo only
        std::vector<ShaderParameterMetadata> parameters;
        std::vector<ShaderParameterMetadata> locals;  // none in this spec (sloinfo prints none today)
    };

    // Describe a shader object without loading or running it. `format` is "rslo", "slo" or nullptr (by magic: LLVM
    // bitcode -> slo, anything else -> rslo). For rslo, `nameOrPath` may be a bare name searched along `searchPath`
    // (colon-separated), as rsloGet does.
    ShaderMetadata describeShaderFile(const char *nameOrPath, const char *format, const char *searchPath);

    // Per-format readers (one per backend; spec 049 adds .oso)
    class IShaderMetadataReader {
        public:
            virtual ~IShaderMetadataReader() = default;
            virtual const char *format() const = 0;
            virtual ShaderMetadata read(const char *nameOrPath, const char *searchPath) const = 0;
    };
}
```

Readers: `rsloMetadataReader.cpp` wraps `rsloGet`/`rsloDelete` (the inspection parser, **in the same image** since Phase 8, unchanged: the `.rslo`/`.sdr`
probing, the `[INFO] rslo: Falling back to .sdr shader` line on stdout); `sloMetadataReader.cpp` wraps
`CLLVMJitEngine::extractMetadataFromFile` (`libRSLJitMeta`, the internal static library that was `libshader_jitmeta`), compiled only with the JIT and **only into the shared `libRSLInfo`**. In a JIT-less build, and in the static archive `libRSLInfo_static` (the `.rslo` half only, no LLVM, the content of today's `libshader_runtime_static`), a `slo`
request answers `Unsupported`.

`libRSLInfo` links `openrendercommon` and (shared, JIT on) `libRSLJitMeta` PRIVATE, and is **never linked into a program that also links `libRSLRuntime`** (formerly `libshader_shading`): the inspection parser and the loader's parser share the flex/bison prefix `rslo` and define the same 37 globals. The guard `Link_InfoRuntimeNotCoLinked` enforces it.

## `sloinfo` after the change

- includes `common/*` headers, `logging.hpp` and `libshader/include/openrender/ShaderMetadata.h` only;
- links `libRSLInfo` only (no `libshader_runtime`/`libshader_jitmeta` link, no LLVM include path; the executable no longer carries LLVM, the dylib does);
- keeps its own argument handling (`--rslo`, `--slo`, `rsloinfo` by `argv[0]`), its format detection by magic, its
  search-path string (`.`, `OPENRENDERHOME/shaders`, `SHADERS`) and every message and exit code:

| Case | Output (unchanged) |
|---|---|
| `.rslo` not found | stderr `Failed to find shader "%s"`, exit 1 |
| `.slo` without metadata | stderr `sloinfo: '%s' is LLVM bitcode but contains no openrender metadata.` + the second line, exit 1 |
| `.slo` with metadata, no JIT entry | stderr warning line, then the listing |
| bitcode in a JIT-less build | stderr `sloinfo: '%s' appears to be LLVM bitcode, but this build was compiled ...`, exit 1 |
| a file that is neither | treated as `.rslo` (`Failed to find shader` unless it parses), as today |

Listing format: byte-identical to today for all 137 shipped shaders in both formats (`tests/shaders/test_sloinfo_output.sh`
and its references unchanged; `Sloinfo_Output137` recorded in Phase 1).

## Spec 043 seam

The structure names no engine (no bytecode index, no LLVM type); the compiler library can publish the same structure at
compile time.

## Library names (L-1)

`libshader_runtime` (the `.rslo` inspection reader this contract wraps) is **renamed into `libRSLInfo`** together with the new
metadata API; it is **not** merged into `libRSLRuntime` (plan.md "Library decision"). Old names kept for one release:
targets `libshader_runtime`, `libshader_runtime_static`, `rslo`, `rslo_static` (ALIAS), exports `openRender::shader_runtime`,
`shader_runtime_static` (aliases), files `libshader_runtime.{dylib,so,a}` (compat symlinks); `libshader_jitmeta` is an ALIAS of the
internal `libRSLJitMeta`; `libshader_metadata` never existed outside this spec (no alias). `libRSLInfo` never links
`libRSLRuntime`.
