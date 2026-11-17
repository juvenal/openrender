# RIB Output Guide

## Overview

openRender provides a standardized RIB output mechanism through the `CRibOut` class. Recent updates have unified the initialization logic and added automatic emission of standard RenderMan RIB headers.

## Standard RIB Headers

When a RIB output is initialized (either via a filename or an existing file stream), the renderer now automatically emits a standard preamble to ensure compliance with the RenderMan RIB-Structure 1.1 specification.

### Example Header

```rib
##RenderMan RIB-Structure 1.1
##Creator openRender 1.0.0
##CreationDate Wed May 20 19:53:31 2026
```

- **RIB-Structure**: Declares compliance with the RIB structure 1.1.
- **Creator**: Identifies the renderer and its version.
- **CreationDate**: Records the timestamp when the RIB was generated.

## Implementation Details

### Unified Initialization

The `CRibOut` constructors have been refactored to use a shared `completeInit()` method. This ensures that the RIB preamble is consistently emitted regardless of whether the output is a file or a pipe/stdout.

### Language Bindings Parity

The **Python** and **Lua** bindings have also been updated to match this behavior. When the `Begin()` method is called in these bindings, the same standard header is written to the output stream, ensuring that RIB files generated from scripts are identical in structure to those generated from C++ or the core renderer.

## Usage

No changes are required from the user side. The headers are emitted automatically upon calling `RiBegin` or its equivalent in the language bindings.

## Option "sampling" (spec 039)

```rib
Option "sampling" "string sampler" ["sobol"]   # "sobol" (default), "pmj02" or "independent"
Option "sampling" "int seed" [0]               # any integer, default 0
```

- The sampler supplies every random number of the render: the camera samples (film jitter, time,
  lens, importance) through camera slots indexed by the absolute subpixel and the frame, and
  every stochastic shading call through a sub-stream of (point, shader, call site, occurrence,
  ray depth). No generator lives in a shading context any more.
- `"legacy"` (internal before Phase 9) is now an unknown name. An unknown name raises
  `CODE_BADTOKEN` (`Unknown sampler "<name>" (valid: sobol, pmj02, independent); keeping the
  current sampler`) and keeps the current value: an earlier valid setting survives, it is not
  reset to the default (GitHub #76, pinned by `SamplingOption_UnknownKeepsCurrent`).
- Shaders query it with `option("sampling:sampler", s)` / `option("sampling:seed", n)`.
- Non-GI renders are bit-identical at any `-t`/`-P`, and a frame rendered alone (`-f n`) equals
  the same frame of the full sequence. GI caches (irradiance cache with `maxerror` > 0, photon
  map) are deterministic too since spec 040; see "Global-illumination determinism" below.
- **Limitation (FR-021):** a uniform `random()` under the raytrace hider is keyed by the shading
  batch, which depends on the bucket, so it MAY change when the bucket size changes.
- A `CropWindow` draws the same samples for a pixel as the full frame (the hiders key subpixels by their position in the full image). A raytrace crop is bit-identical to the same pixels with a 1 x 1 box filter; a reyes crop is not (its dicing changes, GitHub #72), and the raytrace default filter's crop corners are GitHub #70.
- The 8-bit display dither is a hash of (x, y, channel) in `file_base.cpp`.
- User documentation: `docs/site/content/manual/reference/options.md` ("Sampling options"),
  `multithreading.md`, `network-parallel-rendering.md`, `sl-functions.md`.

### Global-illumination determinism (spec 040)

- **The photon map** is the same on every run and at every thread count (`-t:n`): the photons are sorted into a canonical order
  (key, then the bytes of the photon) when the map is balanced, every byte of a written `.gpm` is defined, and a lookup is a pure
  function of the map and the query (the old lookup memo is gone: a render that reads the photon map through `photonmap()` or
  `indirectdiffuse()` is slower than before, GitHub #178 is the follow-up).
- **The irradiance cache** (`occlusion()` and `indirectdiffuse()` with `maxerror` > 0): what a lookup sees is the records read from
  the cache file (`filemode "r"` or `"rw"`) plus the records made earlier by the same shading call (a REYES or zbuffer grid, a raytrace
  shading group). Nothing else is visible until the frame ends, so the image does not depend on the order the grids are shaded in. At
  the end of the frame the records are merged, clamped and written in a canonical order (equal keys commit once), so the `.icache`
  file is byte-identical across runs, thread counts and `-P:n`.
- **The directions** a miss traces are drawn by sample index from the point's own sub-stream: `occlusion()` and `indirectdiffuse()`
  with the cache off take `samples` keyed directions, with the cache on the `nt x np` grid of jittered cells (the gradients need it).
  The accept decision of a cached record is a hash of the call's key and the record's identity, not a draw.
- **Cost:** reusing a cache that an earlier frame wrote (`filemode "r"`), at a fine shading rate, costs about twice as much per later
  frame as it did before spec 040: the cache holds many more, smaller-batch records (8 times as many in the measured scene) and each
  lookup examines more of them (GitHub #179). A single frame is not affected. The two-frame idiom is therefore not recommended as a
  speed-up here until #179 is resolved.
- **Known gap:** under the interpreter (`.rslo`), `indirectdiffuse()` traces no ray (GitHub #173); the `.slo` shaders do.

