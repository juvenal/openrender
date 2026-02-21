# DSO Plugin Example

`dsotest.cpp` demonstrates how to build an external DSO (Dynamically-Shared Object)
shader plugin for openRender.

## What it does

Defines three simple shading operations (`red`, `green`, `blue`) that each return a
solid color. They serve as a minimal reference implementation of the `SHADEOP` /
`SHADEOP_TABLE` API.

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
