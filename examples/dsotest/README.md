# DSO Plugin Example

`dsotest.cpp` demonstrates how to build an external DSO (Dynamically-Shared Object)
shader plugin for openRender.

## What it does

Defines two simple shading operations (`red`, `green`) that each return a solid color.
They serve as a minimal reference implementation of the `SHADEOP` / `SHADEOP_TABLE` API.

## Interface version

The module declares the shadeop interface version it was built against with
`SHADEOP_DECLARE_VERSION()` (once, at file scope, after including `shadeop.h`). The
renderer and the shader compiler read it before calling anything in the module: a MAJOR
version they do not support is refused with a message that names the file, a newer MINOR
loads with a warning, and a module that declares no version is treated as 1.0.0 (it loads
with a warning today and is refused from openRender 1.2.0). The version history is in
`shadeop.h`; the current interface is 1.1.0.

## Threading

A shadeop function may be called concurrently from several shading threads. By default
every call receives the same `initdata`, so a function that writes to it must protect it.
Since interface 1.1 a module can ask for its own data per shading thread: export
`<name>_shadeop_threads[]` (`SHADEOP_THREAD_TABLE`) next to `<name>_shadeops[]`, with a
`SHADEOP_THREAD_INIT` function that is called lazily, once per thread index, and a
`SHADEOP_THREAD_CLEANUP` that runs at `RiEnd`. `red` and `green` keep no state, so they
need neither.

## Building

DSO plugins are built as shared libraries outside the main renderer build. A minimal
`CMakeLists.txt` looks like this:

```cmake
cmake_minimum_required(VERSION 3.20)
project(dsotest)

find_package(openRender REQUIRED)   # provides shadeop.h and algebra.h

add_library(dsotest MODULE dsotest.cpp)
target_link_libraries(dsotest PRIVATE openRender::ri)
set_target_properties(dsotest PROPERTIES PREFIX "")   # no "lib" prefix
```

Build and install to the shader search path specified in your `.rib` file or via the
`SHADERS` environment variable.
