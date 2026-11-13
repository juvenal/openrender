# Plug-in ABI

openRender loads four kinds of plug-in from disk: **display drivers**, **shader DSO
operands** (shadeops), and two kinds of **geometric procedurals** (the `dlo` and
`implicit` modules, plus the RenderMan `DynamicLoad` generators). This document is the
one place that states the rules they share and the rules each family adds. The numbers
it quotes are the version-history tables at the top of each family's header, which are
their single source: `src/display/dsply.h`, `src/shadeop/shadeop.h`,
`src/procedures/dlo.h` and `src/procedures/implicit.h`. The display driver is
written up in full in [DISPLAY_PLUGIN_GUIDE.md](DISPLAY_PLUGIN_GUIDE.md).

---

## Versions at a glance

| Family | Interface version | Version symbol | Function table |
|--------|-------------------|----------------|----------------|
| display | display 2.0.0 | `orDisplayAbiVersion` | yes, `OrDisplayPlugin` |
| shader DSO operands | shadeop 1.1.0 | `orShadeopAbiVersion` | no: `<name>_shadeops[]` and named functions |
| dlo (DLO geometry) | dlo 1.0.0 | `orDloAbiVersion` | no: `dloInit`, `dloIntersect`, `dloTini` |
| implicit surfaces | implicit 1.0.0 | `orImplicitAbiVersion` | no: four named functions |
| DynamicLoad generators | dynamicload 1.0.0 | `orDynamicLoadAbiVersion` (optional) | no: the RenderMan Interface's `ConvertParameters`, `Subdivide`, `Free` |

Version history, one row per released interface (quoted from the headers; a row's tag
names the interface):

| Version | Change |
|---|---|
| 1.0.0 (display) | the Pixie-era display interface; modules without a version are 1.0.0 |
| 2.0.0 (display) | function table, per-display context, `const` snapshot, descriptors, threading contract |
| 1.0.0 (shadeop) | the Pixie-era shadeop interface |
| 1.1.0 (shadeop) | optional `<name>_shadeop_threads[]` with per-thread init and cleanup |
| 1.0.0 (dlo) | the Pixie-era dlo interface |
| 1.0.0 (dynamicload) | the DynamicLoad generator interface, declared optionally |
| 1.0.0 (implicit) | the Pixie-era implicit-surface interface |

These interface versions are independent of the shared library version
(`OPENRENDER_COMPAT_SOVERSION`). Adding `libRIBShadeop` and moving the display
interface to 2.0.0 changed no library's SOVERSION.

---

## The convention

All four headers include `orplugin.h` (`src/include/orplugin.h`, C17 and C++20, installed)
and nothing else from the tree; `orplugin.hpp` is an optional C++20 convenience over it.

**Encoding.** A version is one `uint32_t`: `MAJOR << 16 | MINOR << 8 | PATCH`, built
with `OR_ABI_VERSION(major, minor, patch)` and taken apart with `OR_ABI_MAJOR`,
`OR_ABI_MINOR` and `OR_ABI_PATCH`. MINOR and PATCH have 8 bits: a change that would need
a value above 255 is a MAJOR change, and the version-history check rejects a table that
says otherwise.

**The version symbol.** Each family has one exported function returning its version,
declared with `OR_DECLARE_ABI_VERSION_SYMBOL` (through `OR_DISPLAY_DECLARE_VERSION()`,
`SHADEOP_DECLARE_VERSION()`, `DLO_DECLARE_VERSION()`, `IMPLICIT_DECLARE_VERSION()` and
`OR_DYNAMICLOAD_DECLARE_VERSION()`), once, at file scope. The host calls it before
anything else in the module.

**What the host decides.** One routine, `checkPluginVersion` (`src/common/pluginAbi.h`),
makes the decision for every family and prints at most one diagnostic per module per
process:

| Situation | Result |
|---|---|
| the MAJOR is one the host supports and the MINOR is not newer | loads, silently |
| the MAJOR is supported, the MINOR is newer than the host's | loads, one warning naming both versions |
| the MAJOR is not one the host supports | rejected, one error: the file, the interface, both versions, "rebuild against the installed header" |
| no version symbol | loads as 1.0.0 with one warning (see below); `DynamicLoad` modules load silently |

PATCH never gates loading.

**Function tables.** A family that uses a table (the display) starts it with
`OrTableHeader { structSize, abiVersion }`. The host refuses a table smaller than the
MAJOR's `.0` table before it reads anything else, requires the table's MAJOR to equal
the version symbol's, and reads a field added in a later MINOR only when `structSize`
covers it and the declared MINOR includes it. A struct the host passes to a plug-in
carries its own `structSize` and only grows within a MAJOR: do not read beyond it.

**Status codes.** `OrStatus` is `OR_STATUS_OK` plus `OR_STATUS_ERROR`,
`OR_STATUS_UNSUPPORTED`, `OR_STATUS_BAD_ARGUMENT`, `OR_STATUS_OUT_OF_MEMORY`,
`OR_STATUS_VERSION` and `OR_STATUS_TABLE_SIZE`.

**Ownership and lifetime**, stated once for all families:

1. Whoever allocates frees. The host never frees plug-in memory and a plug-in never
   frees host memory.
2. Host data handed to a plug-in is `const` and valid for a stated window (a display's:
   from `start` until `finish` returns). A plug-in that needs it longer copies it.
3. A plug-in's opaque instance pointer comes back unchanged on every later call and the
   host never dereferences it.
4. Tables and strings a plug-in returns are static and outlive every call.
5. A module is unloaded only after the last call into it has returned and nothing the
   host keeps refers into it.
6. No C++ type, exception or `std::` object crosses the boundary; a plug-in must not let
   an exception escape an entry point.

**Which families use a table, and why not the others.** The display is the one family
the convention shapes completely. The shadeop, dlo and implicit interfaces keep their
Pixie-era shape, a table of declarations or a handful of named functions, because
changing it would be a MAJOR change that breaks every existing module; they take the
version symbol, the checks and the rules above, and add tables only if a later MAJOR
does. `DynamicLoad` generators are defined by the RenderMan Interface (`ConvertParameters`,
`Subdivide`, `Free`), so their entry points are fixed.

**Macro prefixes.** The legacy families keep `SHADEOP_*`, `DLO_*` and `IMPLICIT_*`
deliberately: renaming them would break existing sources for no benefit. The
convention itself and display 2.0.0 use `OR_*` and `Or*`. `LIB_EXPORT` in the old headers
stays as an alias of `OR_PLUGIN_EXPORT`.

**The unversioned sunset.** A module without a version symbol is treated as 1.0.0 and
prints one warning: it will be rejected from openRender 1.2.0. The release name is
one constant, `OR_PLUGIN_UNVERSIONED_SUNSET` in `src/common/pluginAbi.h`. Declare the
version your plug-in was built against, rebuild, and the warning goes. (`DynamicLoad`
generators never warn: the symbol is optional for them.)

---

## Shader DSO operands (shadeop)

The interface is shadeop 1.1.0 (`src/shadeop/shadeop.h`). A module exports a table
`<name>_shadeops[]` of `SHADEOP_SPEC` entries (a definition such as
`"float name(float, point)"`, and the names of an optional init and cleanup function),
terminated by an empty definition, and one function per operand declared with `SHADEOP(f)`.
`examples/dsotest` is a working module.

**The parser and the module code** live in `libRIBShadeop` (`src/shadeop/`), installed as
a shared and a static library with the same SOVERSION as the others. The renderer, the
shading engine and the shader compiler link it, so the compiler (`oshader`) and the
renderer read the same module the same way: one declaration parser, one scan, one
version check.

**Scan order.** Directories are scanned in search-path order; within a directory,
module files in lexicographic order of the file name (not the order the file system
lists them). The first module whose definition parses to the requested name and
prototype and whose exec symbol resolves wins. A module with a missing exec symbol is
reported once, naming the symbol, and the scan continues to later entries and modules.

**Concurrency.** `exec` may be called concurrently from several shading threads. A module
that declares no per-thread entry points receives the same `initdata` on every call, as
before, and must protect anything it mutates. Version 1.1.0 adds an optional table
`<name>_shadeop_threads[]`, parallel to `<name>_shadeops[]`, naming for each operand a
`threadInit(initdata, thread)` and a `threadCleanup(initdata, threaddata)`
(`SHADEOP_THREAD_TABLE`, `SHADEOP_THREAD_INIT`, `SHADEOP_THREAD_CLEANUP`). With them each
thread receives its own data, created lazily on that thread's first call; the slot
array grows when the number of threads rises between frames and keeps existing data.
`threadCleanup` runs at the end of the render (`RiEnd`), possibly on a different thread
from the matching `threadInit`, after every `exec` that used the data has returned and
before the module is unloaded. The compiler loads a module only to read its declarations; it never
calls an operand.

**Signature.** `SHADEOP(f)` declares `void f(void *initdata, int argc, void *argv[])`,
which is exactly `dsoExecFunction`; source that did pointer arithmetic on `argv[i]`
without a cast needs one.

**In a JIT (`.slo`) shader** (spec 038). `oshader --jit` compiles a call of a DSO operand: it
lowers it to the runtime entry `op_dso_call` (`src/libshader/shading/rslOps.h`), which does what the
interpreter does (the operand's data, the per-thread data when the module has any, one `exec`
call per active point, or one for a uniform call), and records the operand's name and prototype
in the module (`openrender.shader.shadeops`). The loader binds each operand with the renderer's
`getDSO` service every time it loads the shader. A module with such a call records the `.slo`
format 2.1.0 (`src/common/sloFormat.h`); every other `.slo` still records 2.0.0, so a renderer
that reads only 2.0 still loads it, and a renderer that reads 2.0 refuses a 2.1 module before it
links it. If the operand cannot be found when the shader loads, the renderer warns
(`Failed to locate DSO function name (prototype)`, in the form of the `.rslo` loader's message,
with the `.slo` format) and falls back to the `.rslo` of the shader, where the interpreter
reports the same missing operand. The shadeop interface and `CRendererServices` do not change
version.

---

## Geometric procedurals

One module in the renderer, `src/procedures/` (`proceduralModules.h`), loads and
releases all three kinds. Each is loaded from the path the renderer located (a bare
name on the `procedural` search path works from any working directory), checked by the
same `checkPluginVersion`, resolved, and reference-counted; the last reference unloads
the module.

**dlo 1.0.0** (`src/procedures/dlo.h`): `dloInit(frame, bmin, bmax)` returns a data
pointer, `dloIntersect(data, from, dir, t, N)` intersects a ray, `dloTini(data)`
releases the data. Used by `Geometry "dlo"`. Declare the version with
`DLO_DECLARE_VERSION()`.

**implicit 1.0.0** (`src/procedures/implicit.h`): four named functions (initialize,
evaluate, evaluate the normal, which is optional, and finish). Used by
`Geometry "implicit"`. Declare the version with `IMPLICIT_DECLARE_VERSION()`.

**DynamicLoad generators** (`RiProcDynamicLoad`): the entry points are the RenderMan
Interface's `ConvertParameters`, `Subdivide` and `Free`, unchanged. Declaring
`OR_DYNAMICLOAD_DECLARE_VERSION()` (in `dlo.h`, version dynamicload 1.0.0) is optional
and an absent declaration is accepted without a warning. The renderer core installs
the hook that expands a generator (`src/ri/parse/procDynamicLoadHook.h`) at `RiBegin`
and removes it at `RiEnd`.

**Lifetime rules.**

- A dlo or implicit instance holds its module until the instance is destroyed.
- A generator's module is held by a **frame reference**, taken at its first expansion in
  the frame and released after that frame's geometry is destroyed, because every
  `RiProcedural` the generator's `Subdivide` creates holds function pointers into the
  module. Across frames each frame acquires and releases it again.
- A generator expanded inside an `RiObjectBegin` definition follows the same rule. A
  generator that itself calls `RiObjectBegin` is outside what the RenderMan Interface
  allows inside a procedural, and is not supported.
- Eight threads acquiring one module produce one load and one version message.

---

## Hider registration and integrators (spec 030): internal until spec 051

Two more families are versioned like the plug-in families above but are **not loaded from disk yet and not installed**: a
hider registers a table (`src/ri/hiders/hiderAbi.h`, hider registration **2.0.0**) and an integrator registers a table
(`src/ri/transport/integratorAbi.h`, integrator **2.0.0**, which also versions the capability vocabulary of
`src/ri/transport/capability.h`). Built-in tables and tables registered from a program (`OR_REGISTER_HIDER`,
`OR_REGISTER_INTEGRATOR`) go through the same single check as a loaded module, the `checkPluginVersion` overload that takes the
version from the table's header (`src/common/pluginAbi.h`): one MAJOR is supported, a newer MINOR registers with one warning, any
other MAJOR is refused with one `CODE_VERSION` error, and a table smaller than the host's is refused too. Neither header is in
`OPENRENDER_PUBLIC_HEADERS` and neither has a version symbol: dlopened hider and integrator modules arrive with spec 051, and the
types a hider's runtime needs (`CHider`, `renderingLoop()`) stay internal C++ until then. Until spec 055 MAJOR stays 2 and 2.x is
unstable: a breaking change steps MINOR and says so in the header's table. Their version histories, quoted from the headers:

| Family | Version | Change |
|---|---|---|
| hider registration | none (1) | hiders were chosen by `strcmp` chains in the renderer core, unversioned |
| hider registration | 2.0.0 | spec 030, starting version confirmed 2026-11-07: the table (name, aliases, provided capabilities, dispatch kind, optional `preDisplaySetup`, `create`/`destroy`) and the reserved pair field of an alias |
| integrator | none (1) | no integrator existed, the `Surface` shader integrated itself |
| integrator | 2.0.0 | spec 030, starting version confirmed 2026-11-07: the table (name, aliases, required and optional capabilities, `create`/`destroy`, `configure`/`prepare`/`finish`), the settings, host and frame structs, the capability vocabulary `rasterization`, `display-output`, `display-channels`, `deep-shadow-output` and `illumination-hook` at 1.0.0 and the reserved names; spec 042 is expected to append `render` and the host services as a MINOR step |

`PluginAbi_DocPaths` covers the installed families only; it is unchanged and passes with this section.

---

## Installing

The public headers (`ri.h`, `ptcapi.h`, `rslo.h`, the four family headers, `orplugin.h`
and `orplugin.hpp`) install into the layout's include directory: `<ORENDERHOME>/include`
for the self-contained layout, the `openRender` subdirectory of the system include
directory for the FHS layout. One CMake list, `OPENRENDER_PUBLIC_HEADERS`
(`CMake/openRenderPublicHeaders.cmake`), is the source of both the install rules and the
checks; `INSTALL_ARTIFACTS.md` is compared with it by `PluginAbi_InstallList`. The
in-tree helper `CFileOutputBase` is not installed.

## Checks

The `plugin_abi` ctest label runs the suite for all of this: the headers compile alone
in C17 and C++20, every version history is parsed and compared with its macros,
`checkPluginVersion` has one definer, the display, shadeop and procedural loaders are
driven with good, newer, wrong-MAJOR and unversioned fixtures, and this document's
paths and version numbers are compared with the tree and the headers
(`PluginAbi_DocPaths`).
