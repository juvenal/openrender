---
title: "SL Functions"
date: 2025-12-10
---

# SL Functions

Placeholder content for SL functions documentation.

## random() and urandom()

`random()` returns a float, a color, a point or a vector of uniform values in [0, 1); `urandom()`
does the same. The values come from the scene's sampler ([Options](../options/#sampling-options)):
each call at each shading point draws from a sub-stream keyed by the point's identity, the shader,
the call site and how many times the call has run at that point. So:

- a point gets the same values whichever thread, bucket or process shades it, and under the
  interpreter (`.rslo`) and the LLVM JIT (`.slo`) alike;
- two calls, or two iterations of one loop, get different values, and adding a call does not
  change what the others draw;
- the derivative of a draw (`Du()`, `Dv()`) is 0.

Each call is a separate sub-stream, so a loop of `random()` calls is not stratified across its
iterations: use the `"samples"` parameter of `trace()`, `transmission()` or `occlusion()` when the
loop is an integral the sampler should stratify.

A **uniform** `random()` (one value for the whole invocation) is keyed by the grid under the reyes
hider and by the shading batch under the raytrace hider; the batch depends on the bucket, so under
raytrace that value may change when the bucket size changes.

