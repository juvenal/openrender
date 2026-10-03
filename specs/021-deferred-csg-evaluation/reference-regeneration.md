# Reference regeneration log: spec 021

Every reference image changed by this feature, with its reason. A reference is
regenerated only when its scene's output changed through a seam improvement or a
result FR-021/FR-025a requires (SC-005). Each change was checked with
before/after 4× crops (nearest-neighbour) and an amplified (×4) difference image.
The crops were kept in the session scratchpad, not in the repository.

## T056: new scenes (implied caps)

| Scene | Status | Evidence |
|---|---|---|
| `csg-open-quadric-caps-raytrace` | new reference | Matches the scene header. The z = −0.5 end cap and the flat sweep face y = 0 carry the cylinder's blue; the gold sphere band shows between them. There are no enclosed see-through pixels, and a 5 px-eroded interior has no outlier pixels (neighbour-median deviation over 25). |
| `csg-open-quadric-caps-reyes` | new reference | Same as raytrace. The curved rim has a sub-pixel gold fringe: the mask keeps a quad if any of its corners is accepted, so a seam can overshoot by up to half a micropolygon (the seam rate is 0.25). `Csg_Caps_reyes` passes. |
| `csg-open-quadric-caps-zbuffer` | new reference | Same as REYES. |

`Csg_Caps_{raytrace,reyes,zbuffer}` (the `--region-chroma` cap checks) were
green before the references were generated.

## T057: existing CSG scenes

These were run against the old (BSP-era) references, sequentially and throttled.

### Unchanged: references kept (12)

`csg-sphere-cube-union-{raytrace,reyes,zbuffer}`,
`csg-sphere-cube-difference-{raytrace,reyes,zbuffer}`,
`csg-sphere-sphere-tight-tolerance-raytrace`,
`csg-subdivision-sphere-union-raytrace`, `csg-nested-tree-4-level-raytrace`,
`csg-object-instance-reuse-raytrace`,
`csg-sphere-sphere-difference-interior-raytrace`,
`csg-sphere-sphere-difference-interior-raytrace-inside`. All of these are within
threshold against the old reference.

### Changed: regenerated (8)

| Scene | Old vs new (worst block, failing blocks) | Reason |
|---|---|---|
| `csg-sphere-cube-intersection-raytrace` | 24.97, 2 | **Seam improvement.** The old seam between the cube face and the sphere was a faceted polyline, the BSP chord of the tessellated sphere. The new one is the smooth true intersection curve, so it moves slightly off the chord. Nothing else differs. |
| `csg-sphere-cube-intersection-reyes` | 21.58, 1 | Same as raytrace. |
| `csg-sphere-cube-intersection-zbuffer` | 25.94, 1 | Same as raytrace. |
| `csg-sphere-sphere-loose-tolerance-raytrace` | 107.91, 33 | **Seam improvement.** The loose `"solid" "tessellationtolerance"` used to bake the second sphere into a visibly faceted, cone-like solid. Operands are no longer tessellated by the solid, so it renders as a true sphere with a smooth seam (FR-023: the attribute is accepted but has no effect). |
| `csg-nurbs-sphere-union-raytrace` | 83.78, 71 | **Required by FR-021 / FR-025a.** The NURBS operand is a single open bicubic sheet. It encloses no volume, so the operand is empty and none of the sheet renders; only the sphere remains. The scene header now says so. |
| `blobby-csg-difference-raytrace` | over threshold along the seam only | **Seam improvement.** The old rim where the sphere is subtracted was jagged, with a dark gap line along it. The new rim is smooth and has no gap. The blobby body is unchanged. |
| `blobby-csg-difference-reyes` | over threshold along the seam only | Same as raytrace. The old reference also showed faint polygon faceting near the seam, which is gone. |
| `blobby-csg-difference-zbuffer` | over threshold along the seam only | Same as REYES. |

After regeneration, all 20 existing scenes and the 3 new cap scenes pass
(`ctest -R 'Visual_(csg-|blobby-csg)'`: 23/23).

**Timing note (not a reference issue):** the first T057 run took about 214 s
(REYES) and 233 s (Z-buffer) throttled for `blobby-csg-difference`, and its
parity pair hit the 360 s timeout in the full suite. A profile showed the cost was
linear scans over the polygonized blobby's thousands of triangles per
classification query. That is fixed by the operand bound hierarchy
(`CCSGResolvedOperand::buildIndex()`; see the tasks.md Phase 3 notes). Measured
like for like afterwards (`nice` only, back to back, wall clock): REYES 0.73 s
against master's 4.59 s, Z-buffer 0.40 s against 4.57 s, raytrace 0.20 s against
6.19 s. The regenerated references were re-verified after the fix: all 82
CSG/blobby-CSG tests pass.
