---
title: "openRender Documentation"
date: 2026-08-29
weight: 1
---

# Welcome to openRender Documentation

openRender is an open-source, RenderMan-compliant photorealistic renderer
written in C++20 and released under the LGPL 2.1. It reads RIB scene files,
renders them with either a REYES micropolygon hider or a ray tracer, and
shades them with its own RenderMan Shading Language implementation — either
through a bytecode interpreter or an LLVM JIT backend.

The project lives on GitHub at
[github.com/juvenal/openrender](https://github.com/juvenal/openrender). It
evolved from Pixie, originally written by Okan Arikan, and remains LGPL 2.1.

## Getting openRender

There is no binary download. Build from source:

```bash
git clone https://github.com/juvenal/openrender.git
cd openrender
cmake -S . -B build
cmake --build build --config Release
```

Full prerequisites and platform notes are in
[Installing / running openRender](/openrender/manual/reference/installing-and-running/),
and in `INSTALL.md` in the repository.

## Main Sections

- [Documentation](/openrender/manual/) - Documentation and reference on openRender's features
- [Tutorials](/openrender/manual/#tutorials) - Tutorial-style / How-To guides for openRender
- [FAQ](/openrender/development/faq/) - Frequently Asked Questions

Corrections and additions are welcome as pull requests against the
repository — see
[Contributing](/openrender/development/contributing/). The documentation
sources live under `docs/site/content/`.

## Documentation

How openRender relates to the RiSpec, and documentation on openRender's non-standard features and extensions.

- [Installing / running openRender](/openrender/manual/reference/installing-and-running/)
- [Multithreading](/openrender/manual/reference/multithreading/)
- [Hiders](/openrender/manual/reference/hiders/)
- [Display drivers](/openrender/manual/reference/display-drivers/)
- [Options](/openrender/manual/reference/options/)
- [Attributes](/openrender/manual/reference/attributes/)
- [Solid CSG Operations](/openrender/manual/reference/solid-csg-operations/)
- [Blobby Implicit Surfaces](/openrender/manual/reference/blobby-implicit-surfaces/)
- [Subdivision Surfaces](/openrender/manual/reference/subdivision-surfaces/)
- [NURBS Trim Curves](/openrender/manual/reference/nurbs-trim-curves/)
- [Occlusion culling](/openrender/manual/reference/occlusion-culling/)
- [Baking 3D Textures](/openrender/manual/reference/baking-3d-textures/)
- [Network parallel rendering](/openrender/manual/reference/network-parallel-rendering/)
- [DSO shading](/openrender/manual/reference/dso-shading/)
- [Transparency shadow maps](/openrender/manual/reference/transparency-shadow-maps/)
- [Global illumination](/openrender/manual/reference/global-illumination/)
- [Point based occlusion and color bleeding](/openrender/manual/reference/point-based-gi/)
- [Raytracing in SL](/openrender/manual/reference/raytracing-in-sl/)
- [Raytraced shadows / reflections](/openrender/manual/reference/raytraced-shadows-and-reflections/)
- [Hardcoded shaders](/openrender/manual/reference/hardcoded-shaders/)
- [Shader library](/openrender/manual/reference/shader-library/)
- [Version management](/openrender/manual/reference/version-management/)
- [Performance / Quality Tips](/openrender/manual/reference/performance-and-quality-tips/)
- [Source at a Glance](/openrender/manual/reference/source-at-a-glance/)
- [Using openRender with Maya](/openrender/manual/reference/using-openrender-with-maya/)
- [Conditional RIB](/openrender/manual/reference/conditional-rib/)
- [RIB Resources](/openrender/manual/reference/rib-resources/)
- [Ptc API](/openrender/manual/reference/ptc-api/)
- [User Attributes And Options](/openrender/manual/reference/user-attributes-and-options/)
- [SL Functions](/openrender/manual/reference/sl-functions/)

For what is and is not implemented against the specification, see
[openRender and the RiSpec](/openrender/references/renderman-rispec/).

## Examples / Tutorials

Tutorial-style guides to various features in openRender.

- [Basics, Running openRender](/openrender/manual/tutorials/basics-running-openrender/)
- [Raytraced shadows](/openrender/manual/tutorials/raytraced-shadows/)
- [Soft raytraced shadows](/openrender/manual/tutorials/soft-raytraced-shadows/)
- [Global Illumination](/openrender/manual/tutorials/global-illumination/)
- [Dispersion](/openrender/manual/tutorials/dispersion/)
- [Baking To Textures](/openrender/manual/tutorials/baketotexture/)

## What is in openRender 1.0.0

Version 1.0.0 is in development; there is no tagged release yet. What the
tree currently carries, beyond what it inherited:

**Shading.** An LLVM JIT backend for the shading language: `oshader --jit`
compiles RSL to bitcode that runs through the same `op_*` ABI the bytecode
interpreter uses, selectable per-primitive, scene-wide, or at build time.
Imager shaders are implemented with all seven specification variables, in the
specification's pipeline order (render, exposure, imager, quantize).

**Geometry.** Four capabilities that were previously stubs or partial
implementations:

- [Solid CSG operations](/openrender/manual/reference/solid-csg-operations/) —
  all three set operations, arbitrarily nested, with any primitive as an
  operand
- [Subdivision surfaces](/openrender/manual/reference/subdivision-surfaces/) —
  Catmull-Clark and Loop schemes, the full tag set, hierarchical per-face
  overrides, and cross-hider motion blur
- [NURBS trim curves](/openrender/manual/reference/nurbs-trim-curves/) — with
  one classification test shared between the REYES and ray-trace paths
- [Blobby implicit surfaces](/openrender/manual/reference/blobby-implicit-surfaces/)
  — all four primitive-field and all eight combining opcodes

**Hiders.** The REYES and ray-trace hiders now share their sampling,
compositing and pixel-filter kernels, so they converge on motion blur,
transparency, matte objects, displacement and depth-filter modes rather than
each implementing them separately. See
[Hiders](/openrender/manual/reference/hiders/).

**Tools.** `orender-wire`, an interactive wireframe scene previewer (Metal on
macOS, GTK 4 on Linux); a platform-neutral IPC framebuffer display; and
Python and Lua bindings for driving the interface from a script.

Earlier release notes, from before the project moved to GitHub, are kept in
[Releases](/openrender/development/releases/).
