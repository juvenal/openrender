---
title: "Plug-in ABI"
date: 2026-10-31
---

# Plug-in ABI

openRender loads display drivers, shader DSO operands and geometric procedurals from
modules on disk. Each kind carries an interface version, and the renderer checks it
before it calls anything in the module. This page is for the author of such a module:
what the versions mean, what is shared between the kinds, what warning an old module
gets, and how to move a display driver to the current interface. The full rules are in
`DEVNOTES_DETAILS/PLUGIN_ABI.md` and `DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md` in the
source tree.

## Versioning rules

An interface version is `MAJOR.MINOR.PATCH`. The current versions are:

| Kind of plug-in | Interface | Declared with |
|---|---|---|
| Display driver | display 2.0.0 | `OR_DISPLAY_DECLARE_VERSION()` in `dsply.h` |
| Shader DSO operand | shadeop 1.1.0 | `SHADEOP_DECLARE_VERSION()` in `shadeop.h` |
| DLO geometry | dlo 1.0.0 | `DLO_DECLARE_VERSION()` in `dlo.h` |
| Implicit surface | implicit 1.0.0 | `IMPLICIT_DECLARE_VERSION()` in `implicit.h` |
| `DynamicLoad` generator | dynamicload 1.0.0 | `OR_DYNAMICLOAD_DECLARE_VERSION()` in `dlo.h` (optional) |

Declare the version once, at file scope, and rebuild against the installed header when
you change it. The renderer then decides by MAJOR:

- a MAJOR it supports loads, silently;
- a MINOR newer than the renderer's loads with one warning;
- any other MAJOR is rejected, with one error naming the file and both versions;
- PATCH never matters.

A MAJOR changes only when the interface breaks for existing modules. A MINOR adds
something a module can ignore, and a PATCH fixes something without changing the
interface. These numbers are independent of the version of the renderer's own shared
libraries: neither the new `libRIBShadeop` library nor display 2.0.0 changed it.

## What the kinds share

Every header includes `orplugin.h`, which defines the version encoding, the export
macros, the status codes and the header every function table starts with; `orplugin.hpp`
adds an optional C++ convenience. The display interface is a function table
(`OrDisplayPlugin`). The shadeop, dlo and implicit interfaces keep their original shape,
a table of declarations or a few named functions, so that existing modules keep loading.

Ownership is the same everywhere: whoever allocates frees; what the host hands you is
`const` and valid for a stated window; no C++ exception may leave an entry point.

## Modules without a version

A module that declares no version is treated as 1.0.0 and prints one warning per module
per process. **Unversioned modules will be rejected from openRender 1.2.0.** Declare the
version and rebuild before then. (`DynamicLoad` generators never warn: the declaration is
optional for them.)

## Migrating a display driver to 2.0.0

A display driver written for the old interface (`displayStart`, `displayData`,
`displayRawData`, `displayFinish`) still loads, through an adapter, but it changes in
three ways: pointers its parameter callback returns are copies, so writes through them are
lost; the callback answers only inside `displayStart`; and `displayRawData` is ignored.
To move it to 2.0.0:

1. Include `dsply.h` and declare the version with `OR_DISPLAY_DECLARE_VERSION()`.
2. Export `orDisplayPlugin`, returning a static `OrDisplayPlugin` table of `start`,
   `data` and `finish`.
3. Read the parameters you need from the `OrDisplayInfo` that `start` receives, with
   `orDisplayFindParameter`, instead of calling a callback later.
4. Write each tile in `data`; tiles are disjoint and may arrive from several threads at
   once. Free your instance in `finish`, which runs once after the last tile.

`examples/display/simpledisplay.c` is a complete driver that follows these steps and
writes a PPM; `examples/display/README.md` has the compile line. The headers it needs
are installed in `<ORENDERHOME>/include` for the self-contained layout and in the
`openRender` subdirectory of the system include directory for the FHS layout, and
`find_package(openRender CONFIG)` supplies the right one to a CMake build.
