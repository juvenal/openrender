---
title: "Network Parallel Rendering"
date: 2025-12-10
---

# Network Parallel Rendering

Placeholder content for network parallel rendering documentation.

## Determinism across processes

A render split over processes (`orender -P:n`) or network servers produces the same image as a
single-process render: the samples of a pixel are keyed by the pixel's absolute raster position
and the frame, not by which process or bucket renders it (see
[Options](../options/#sampling-options)). Global illumination is deterministic too (spec 040):
a server sends the irradiance records it made, the client commits them in a canonical order, and
the cache file written by a `-P:n` render equals the one a single process writes.

