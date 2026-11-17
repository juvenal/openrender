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
the 8-bit dither. Global illumination is deterministic too (spec 040): the irradiance cache
(`occlusion()`/`indirectdiffuse()` with `maxerror` > 0) is built from the records each shading
batch makes and committed in a canonical order at the end of the frame, and the photon map is
written in a canonical order, so the image and the cache and map files are the same at any thread
count and run to run. A thread pin is no longer needed for them. Reusing a cache written by an
earlier frame at a very fine shading rate costs about twice as much per later frame as it used to
(GitHub #179).

