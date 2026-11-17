# Draft: spec 040 edits to `DEVNOTES.md`

`DEVNOTES.md` is not edited by this spec (M6): the coordinator applies these at merge preparation, on top of whatever the other specs have written there. Nothing here depends on that
application.

## 1. Status table: a new row after "GI cache split (spec 032)"

```
| GI determinism (spec 040) | Implemented through Phase 8 (see `specs/040-gi-determinism/validation.md`: results on macOS and on Linux, Ubuntu 24.04, GCC 13 `-Werror`, 2026-11-17). The photon map and the irradiance cache are the same on every run, at any `-t:n` and under `-P:n`: the photon store's claim and reset are one step under its lock (the intermittent `CPhotonHider` crash, #164; `delete[]`, #165), the photons are sorted into a canonical order and every byte of a `.gpm` is defined (#163), the lookup memo is gone (#160), the cache is a read-only fixed part plus a Shading batch scope plus a canonical commit with hashed accept values and index-drawn directions, and a `-P:N` server sends only the records it made (#161). 34 GI entries join the determinism sweep (3 of 3 repetitions on both platforms), the GI thread pins are removed, the GI references were re-rendered once, thresholds only lowered. `kRendererServicesVersion` 2.6.0. Known: the interpreter's `indirectdiffuse()` traces no ray (#173); the photon lookup is slower without its memo (+720 % on a photon-lookup-heavy render, #178 is the follow-up); the cache is 8 times as dense, so reusing it at a fine shading rate costs about 2x per later frame (#179); `perf-photon-pass` +21 % on macOS. D5: no deterministic-prepass spec (M3) for now; revisit after #179. |
```

## 2. Open issues

Untouched by this spec: #158 (`jitOcclusionBatch`), #159, #166, #122, #145 (`photonmap()` interpreter and JIT output), #102. Filed by this spec, not fixed here: #170 (the photon pass under `-P:N`), #173
(`indirectdiffuse()` under the interpreter), #178 (precomputed irradiance at the photons: the follow-up for the photon lookup's cost), #179 (cache density). Fixed: #160, #163, #164, #165; #161 is
subsumed by the commit (the maintainer closes each at merge only if its test passed on both platforms: see `validation.md` section 6).
