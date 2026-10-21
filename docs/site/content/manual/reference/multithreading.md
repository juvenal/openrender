---
title: "Multithreading"
date: 2025-12-10
---

# Multithreading

Placeholder content for multithreading documentation.

## Determinism across thread counts

`orender -t:n` sets the number of rendering threads (by default one per CPU core). The image does
not depend on it: every camera sample and every stochastic shading call draws from a sub-stream
keyed by the sample's identity (see [Options](../options/#sampling-options)), never from a
per-thread random stream, so a render at `-t:1` and one at `-t:8` are bit-identical, including
the 8-bit dither. The exception is global illumination through the irradiance cache
(`occlusion()`/`indirectdiffuse()` with `maxerror` > 0) and the photon map, which are filled in the
order points happen to be shaded; render those single-threaded (`Option "limits" "numthreads" [1]`)
when you need them reproducible, until spec 040 makes them order-independent.

