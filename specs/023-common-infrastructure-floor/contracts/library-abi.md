# Contract: libraries, exported symbols and binary compatibility (spec 023)

## 1. Library set after this feature

| Library | Kind | New / changed | Provides |
|---|---|---|---|
| `libRIBCommon.1.dylib` / `libRIBCommon.so.1` (+ dev symlink) | shared, `SOVERSION ${OPENRENDER_COMPAT_SOVERSION}` | **new** | Everything in `src/common`: the existing utilities plus diagnostics, arena, stats, ref-count/file-resource bases, sampling, variable parser, RI tokens, renderer vocabulary, `current_log_level` |
| `libRIBCommon.a` | static | contents grow | Same objects, for fully static programs only |
| `libRIBRender` / `libRIBVector` / `libRIBOut` (shared) | shared | link + **re-export** `libRIBCommon` (macOS); `DT_NEEDED` (Linux) | no longer define any moved symbol |
| `libRIBRender.a` / `libRIBVector.a` / `libRIBOut.a` | static | no longer contain the moved objects | `RIBRender_static` requires **shared** `libRIBCommon`; the other two require `libRIBCommon.a` |
| `libshader_shading` | shared | links `libRIBCommon` (no longer embeds the archive) | infrastructure bound two-level to `libRIBCommon` (macOS) |
| `libshader_compiler`, `libshader_runtime` (shared) | shared | link `libRIBCommon` instead of embedding it | — |
| `libshader_*_static` | static | depend on `libRIBCommon.a` | — |
| Display modules (`file`, `framebuffer`, `rgbe`, and `openexr` when built) | shared modules | link `libRIBCommon` instead of embedding it | share `current_log_level` with the host; the installed rpath must find `libRIBCommon` |

The compatibility symlinks (`libri`, `libribVector`, `libribOut`) are unchanged.
`libRIBCommon` needs no alias, because it has never had another name.

## 2. Single-definer invariant (FR-008)

Across every image built in the tree, each symbol below is **defined in exactly one image,
`libRIBCommon`**:
- `globalArena`, `stats`, `current_log_level`;
- the diagnostic sink slot;
- every `RI_*` token;
- `error`, `warning`, `fatal`, `info`, `installDiagnosticSink`, `diagnosticsPrintDefault`;
- `parseVariable`;
- `memoryInit`, `memoryTini`, `memoryNewPage`, `memoryDeletePage`;
- `sampleHemisphere`, `sampleCosineHemisphere`;
- the `CRefCounter` and `CFileResource` constructors, destructors, vtables and typeinfo.

`Common_StateOwnership` enforces it:
- **macOS**: `nm -gU` on every image.
- **Linux shared objects**: `nm -D --defined-only`.
- **Linux executables**: a definition counts as a reference when `readelf -rW` shows an
  `R_*_COPY` relocation for that symbol, because PIE copy relocations put a copy in the
  executable's `.bss`. Any other definition in an executable is a violation.

Fully static programs (no shared openRender image, see §1) hold exactly one copy in the
executable, and are excluded by name.

**Link-time guarantee**: `libRIBCommon` links with `-Wl,-undefined,error` (macOS) and
`-Wl,--no-undefined` (Linux). It references no symbol from any other openRender image.

## 3. Shading-library binding (SC-002)

Each of the 17 infrastructure symbols the shading library imports must resolve from
`libRIBCommon`, never from `libRIBRender`. The 17 are:
- the `CRefCounter` and `CFileResource` constructors, destructors and typeinfo;
- `memoryInit`, `memoryNewPage`, `memoryTini`;
- `stats`;
- `sampleHemisphere`, `sampleCosineHemisphere`;
- `error`, `warning`, `info`;
- `parseVariable`;
- the global arena.

How each platform shows it:
- **macOS**: in `nm -m libshader_shading.1.dylib`, every one of the 17 is either defined in the
  library or listed as `(from libRIBCommon)`. None is `(dynamically looked up)`. The remaining
  flat imports are exactly DB-001's 18 lines, diffed against
  `$S/023-libshader-undef-before.txt`.
- **Linux**: `readelf -d libshader_shading.so.1` lists `NEEDED libRIBCommon.so.1`, and
  `nm -D --defined-only libRIBRender.so.1` defines none of the 17. At run time,
  `LD_DEBUG=bindings` shows them bound to `libRIBCommon.so.1`.

## 4. Binary compatibility (FR-014, SC-006)

- A binary linked against the pre-feature `libRIBRender`/`libRIBVector`/`libRIBOut` runs against
  the new ones without relinking.
  - macOS: two-level bindings to a moved symbol resolve through the RI library's
    `LC_REEXPORT_DYLIB libRIBCommon`. It must be present in all three libraries, checked with
    `otool -l`.
  - Linux: binding goes through the global scope via `DT_NEEDED`.
- **Source compatibility**: `ri.h` is byte-identical, every `RI_*` token keeps its name and value,
  and `RiErrorHandler`/`RiErrorPrint`/`RiErrorIgnore`/`RiErrorAbort` behave as before.
- **SOVERSION** stays at `${OPENRENDER_COMPAT_SOVERSION}` for every existing library. Nothing is
  removed from what a consumer can reach through the library it already links.

## 5. CMake package

- `openRenderTargets` adds `openRender::RIBCommon`, the shared library. The shared RI targets link
  it privately; the re-export makes it visible to consumers.
- `openRenderStaticTargets` exports the archive as `openRender::RIBCommon_static`.
  - `openRender::RIBRender_static` has `INTERFACE_LINK_LIBRARIES` pointing to
    `openRender::RIBCommon` (shared). The `Static` component therefore requires the shared
    targets' file.
  - Its `-force_load` / `--whole-archive` handling of `libRIBRender.a` is unchanged.
- Install: the shared library goes to `${CMAKE_INSTALL_LIBDIR}` next to the RI libraries.
  `orender-wire.app/Contents/Frameworks`, the self-contained install and the release bundling also
  include it.
