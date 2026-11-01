# Proposed CLAUDE.md changes for spec 037 (versioned plug-in ABIs)

Draft for the maintainer. `CLAUDE.md` itself is **not** edited by the spec work; each block below names
the section it replaces or extends. Gotcha numbers assume the main checkout's `CLAUDE.md` (it has
gotchas 1 to 26 after spec 028); the worktree's copy stops at 25.

---

## 1. Repository layout (extends the tree under "## Repository layout")

Add these entries (keep the tree's alignment; `src/display/` is not shown in the current tree, add
it with them):

```
│   ├── display/            Display Outputs: dsply.h (display interface 2.0.0), file/framebuffer/openexr/rgbe drivers
│   ├── shadeop/            Shader DSO Operands → libRIBShadeop (shadeop.h 1.1.0, declaration parser, scan, per-thread data)
│   ├── procedures/         Geometric Procedures: dlo.h, implicit.h, the one loader for dlo/implicit/DynamicLoad
│   ├── include/            orplugin.h / orplugin.hpp, the shared plug-in convention (installed)
```

and extend the `common/` line with `plug-in version check (pluginAbi.{h,cpp})`.

## 2. Architecture (new subsection after "### Texture system (spec 027)")

```
### Plug-in ABIs (spec 037)
Every plug-in family declares an interface version, `OR_ABI_VERSION(major, minor, patch)` =
`MAJOR<<16 | MINOR<<8 | PATCH` (MINOR and PATCH are 8 bits; a change that needs more is a MAJOR
change), through an exported query symbol (`orDisplayAbiVersion`, `orShadeopAbiVersion`,
`orDloAbiVersion`, `orImplicitAbiVersion`, optional `orDynamicLoadAbiVersion`) declared by a
`*_DECLARE_VERSION()` macro. The version-history table in each family header's comment is the single
source of its numbers; `DEVNOTES_DETAILS/PLUGIN_ABI.md`, the display guide and the site page quote it
and `PluginAbi_DocPaths` compares them. One routine, `checkPluginVersion` (`src/common/pluginAbi.cpp`,
`libRIBCommon`), makes every load decision: a supported MAJOR loads, a newer MINOR loads with one
warning, any other MAJOR is rejected, and a module without the symbol loads as 1.0.0 with one warning
and is rejected from openRender 1.2.0 (`OR_PLUGIN_UNVERSIONED_SUNSET`); DynamicLoad never warns.
Display 2.0.0 is a function table (`OrDisplayPlugin`: `start`/`data`/`finish`) over a per-display
context and a `const` `OrDisplayInfo` snapshot; `CDisplayHost` (`src/ri/render/displayHost.*`) loads
and drives it, and MAJOR 1 modules and `RiCustomDisplay` run through the confined adapter
(`displayV1Adapter.*`, the only files that name a v1 display type). Shader DSO operands live in
`libRIBShadeop` (`src/shadeop/`, shared and static like `libRIBCommon`): one hand-written declaration
parser (the flex/bison `dso` grammar is gone), one scan, per-thread operand data (`shadeop` 1.1.0).
`src/procedures/` is the one loader for `dlo`, `implicit` and DynamicLoad modules, reference-counted,
with a frame reference that keeps a DynamicLoad generator's module until the frame's geometry is
destroyed (the hook is installed at `RiBegin` like the services). Texturing reaches the AOV channel
registry through `CTexture3dChannelSource` (`texture3dChannels.*`), not the renderer core. The legacy
families keep their `SHADEOP_*`/`DLO_*`/`IMPLICIT_*` macro prefixes on purpose; the convention and
display 2.0.0 use `OR_*`/`Or*`. The public headers come from one list,
`OPENRENDER_PUBLIC_HEADERS` (`CMake/openRenderPublicHeaders.cmake`), installed into
`OPENRENDER_INCLUDE_INSTALL_DIR` (`<ORENDERHOME>/include` self-contained, `include/openRender` FHS);
`file_base.h` is not installed. See `DEVNOTES_DETAILS/PLUGIN_ABI.md`.
```

## 3. Testing (add to the code block under "## Testing")

```
ctest --test-dir build -L plugin_abi -j4                # plug-in ABIs (spec 037): headers, versions, display host and v1 adapter, shadeop module, procedures, install layouts, doc checks
```

and one sentence after the block:

```
The `plugin_abi` references (`tests/plugins/**/reference`) are captured once on the pre-change tree;
`cmake --build build --target plugin-abi-capture-baselines` writes a missing one and refuses to
overwrite an existing one unless configured with `-DOR037_RECAPTURE=ON`. `-DOPENRENDER_TSAN=ON`
(forces `OPENRENDER_ENABLE_JIT=OFF`) builds with ThreadSanitizer; run `-L plugin_abi` in that tree
(use a separate build directory).
```

## 4. Build options (extends the "Key CMake options" sentence under "## Build")

Add `OPENRENDER_TSAN` (ThreadSanitizer, off by default) and `OR037_RECAPTURE` (test baselines).

## 5. Known gotchas (new entry 27, after entry 26)

```
27. **Plug-in modules are `dlopen`ed images; test and write them accordingly (spec 037).**
   A loaded image runs its destructors at process exit, so a test that checks "unloaded after
   `finish`" asserts the order of log lines, not that a destructor ran at that moment. The test
   fixtures are plain C with no thread-local storage (a TLS block would change what an unload
   releases). The v1 adapter's `thread_local` display binding is scoped to one `displayStart` call
   and is not a "current display": nothing in the host reads a global one (there is none), and a
   lookup outside `displayStart` answers `NULL` and logs once. The shadeop scan order is
   lexicographic by module file name within a directory, in search-path order across directories,
   and the renderer and `oshader` share the one scan; `osEnumerate` order is never relied on. A
   DynamicLoad generator's module is held by a frame reference released after `root->destroy()`.
   `checkPluginVersion` is the only version check: a new family adds an `SPluginFamily`, never a
   second check.
```

## 6. Active Technologies (append)

```
- C++20 and C17 (the public plug-in headers and the example driver are strict C17), CMake ≥3.19,
  `-Werror`; no new dependencies — a shared plug-in convention (`orplugin.h`), versioned display,
  shadeop, dlo and implicit interfaces, `libRIBShadeop`, the Procedures module
  (037-plugin-abi-semver)
- Storage: N/A — no new persistent state; the `plugin_abi` test references are captured files
  (037-plugin-abi-semver)
```

## 7. Recent Changes (prepend)

```
- 037-plugin-abi-semver: every plug-in family declares an interface version checked by one routine
  (`checkPluginVersion`); display 2.0.0 (function table, per-display context, `const` snapshot,
  channel descriptors, a documented threading contract) loaded by `CDisplayHost`, with MAJOR 1
  drivers and `RiCustomDisplay` behind a confined adapter; shadeop 1.1.0 (per-thread data) in the
  new `libRIBShadeop`; dlo, implicit and DynamicLoad at 1.0.0 behind one Procedures module with a
  frame-held generator module; Texturing reads the channel registry through its own interface. DB-003,
  DB-014 and DB-021 resolved (6 allowlist lines deleted, none added). Unversioned plug-ins warn and
  are rejected from openRender 1.2.0; `file_base.h` is no longer installed; the public headers come
  from one list into the layout's include directory (see spec 037).
```
