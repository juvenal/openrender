---
title: "DSO Shading"
date: 2025-12-10
---

# DSO Shading

Placeholder content for DSO shading documentation.

## Plug-in versions

Shader DSO operands declare an interface version, and the renderer checks it when it
loads the module. The versioning rules, the per-thread data added in shadeop 1.1.0 and
the warning an unversioned module receives are on the [Plug-in ABI](../plugin-abi/) page.

## DSO operands in JIT (`.slo`) shaders

A shader compiled with `oshader --jit` can call a DSO operand in the same way as an interpreter
(`.rslo`) shader; the image is the same. Compile it with the directory of the operand's module on
the search path of `oshader` (`oshader --jit -I <module directory> shader.sl`). When the renderer
loads the shader it looks every operand up again, so the module must be on the procedural search
path of the render. If an operand is missing, the renderer warns that it failed to locate the DSO
function and uses the shader's `.rslo` instead, which reports the missing operand as before; keep
both formats of a shader if it may be rendered without its module. A `.slo` that calls an operand
needs a renderer new enough to read its format; every other `.slo` is unchanged.
