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
[Options](../options/#sampling-options)). As with threads, global illumination through the
irradiance cache or the photon map is the exception until spec 040.

