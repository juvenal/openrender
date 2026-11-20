# RenderMan RIB Scene Research & Showcase Suite

This directory (`/Users/juvenal/Projects/RenderMan/scenes`) contains both a reference guide to external RenderMan Interface Bytestream (`.rib`) scene archives and a comprehensive, self-contained **RI Spec v3.2 Showcase Scene Suite** designed to test and demonstrate advanced geometric primitives and camera effects.

---

## Part 1: External RIB Scene Sources & Archives

When testing a custom renderer that targets **Classic RI Spec v3.2** (with RIS extensions), here are the best external sources of `.rib` files on the internet:

### 1. Open-Source Renderer Conformance & Regression Suites
* **Aqsis Renderer (`aqsis/aqsis` on GitHub / SourceForge)**
  * **Directories:** `examples/` and `testing/regression/RIBs/`
  * **Features Tested:** Exhaustive unit tests for Quadrics (`Sphere`, `Cone`, `Cylinder`, `Hyperboloid`, `Paraboloid`, `Torus`), Bicubic/Bilinear Patches (`Patch`, `PatchMesh`), `NuPatch` with `TrimCurve`, `SubdivisionMesh` (including creases, corners, holes, and `interpolateboundary`), `Blobby` implicit surfaces, CSG (`SolidBegin`/`SolidEnd`), `MotionBegin`/`MotionEnd`, and `DepthOfField`.
  * **Notable Scenes:** `vase.rib`, `pirate.rib`, `fisheye.rib`, `cell.rib`.
* **Pixie Renderer (`okanari/Pixie` on GitHub)**
  * **Features Tested:** Classic Reyes + Raytracing hybrid RIBs, shadow maps, global illumination, photon mapping, and standard quadric/subdivision tests.
* **Psychopath Renderer (`cessen/psychopath` on GitHub)**
  * **Features Tested:** Clean, modern path-traced RIB scenes testing bilinear/bicubic patches, heavy motion blur, and depth of field.

### 2. Educational & Book Archives
* **Prof. Malcolm Kesson’s *Fundza* (`https://www.fundza.com/`)**
  * Clean, hand-crafted ASCII `.rib` examples covering hierarchical transforms, `ReadArchive`, `ObjectBegin`/`ObjectInstance`, coordinate systems, and procedural primitives.
* **SIGGRAPH Course Notes (1990–2004)**
  * *SIGGRAPH 2003 Course 9 (RenderMan: Theory and Practice)* and *Advanced RenderMan (Apodaca & Gritz)* include classic production examples such as the *Killeroo* subdivision mesh and procedural shaders.

### 3. Modern RIS / Multi-Renderer Benchmarks
* **Pixar RenderMan Community Resources (`https://renderman.pixar.com/`)**
  * Official Pixar scenes (*Walking Teapot*, *Speeder Bike*, *Kitchen*). Geometry is often stored in `.rib` archives (`PointsGeneralPolygons`, `SubdivisionMesh`, `NuPatch`) while shading uses modern RIS `Bxdf` (`PxrSurface`, `PxrDisney`) and `Integrator` (`PxrPathTracer`).
* **Benedikt Bitterli’s Rendering Resources (`https://benedikt-bitterli.me/resources/`)**
  * Famous GI/path-tracing benchmark scenes that can be exported to `.rib` via Blender/PBRT exporters.

---

## Part 2: Local Showcase Scene Suite Overview

All scenes in this folder are self-contained, human-readable ASCII `.rib` files adhering to **RI Spec v3.2** (with optional RIS comments/hooks) designed to showcase your renderer's specific capabilities:

### 1. Trimmed NURBS (`NuPatch` + `TrimCurve`)
* [`01_nurbs_swiss_cheese_dome.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/01_nurbs_swiss_cheese_dome.rib) — A billowing bicubic NURBS canopy pierced by multiple circular and star-shaped parametric `TrimCurve` loops, casting intricate perforated shadows onto a pedestal.
* [`02_nurbs_twisted_ribbon_lantern.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/02_nurbs_twisted_ribbon_lantern.rib) — Nested rational NURBS cylinders and bowls with diagonal slit trim curves and nested island trim loops enclosing an glowing core.
* [`03_nurbs_arch_pavilion.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/03_nurbs_arch_pavilion.rib) — Architectural hyperbolic-paraboloid NURBS vaults with gothic-arch boundary trims and circular oculus cutouts.
* **Camera/Time Variations:**
  * [`01_nurbs_swiss_cheese_dome_dof.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/01_nurbs_swiss_cheese_dome_dof.rib) — Shallow depth-of-field closeup focusing on the middle trimmed edge.
  * [`02_nurbs_twisted_ribbon_lantern_mblur.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/02_nurbs_twisted_ribbon_lantern_mblur.rib) — Rotational `MotionBegin [0 1]` spinning the trimmed lantern slits into light streaks.

### 2. Constructive Solid Geometry (`SolidBegin` / `SolidEnd`)
* [`04_csg_wiktor_cube_lens.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/04_csg_wiktor_cube_lens.rib) — Classic recursive hollow mechanical cube (`difference` of a rounded `intersection` box and three orthogonal `union` cylinders) housing a bi-convex lens (`intersection` of two spheres).
* [`05_csg_planetary_gear_chronometer.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/05_csg_planetary_gear_chronometer.rib) — Precision horological gear assembly built entirely from boolean `union`, `intersection`, and `difference` trees of cylinders, cones, tori, and spheres.
* [`06_csg_alchemist_crucible.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/06_csg_alchemist_crucible.rib) — Nested concentric spherical shells with boolean-carved viewing windows revealing internal helical torus rings.
* **Camera/Time Variations:**
  * [`04_csg_wiktor_cube_lens_dof.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/04_csg_wiktor_cube_lens_dof.rib) — Macro array of three CSG cubes in depth with strong bokeh (`DepthOfField`).
  * [`05_csg_planetary_gear_chronometer_mblur.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/05_csg_planetary_gear_chronometer_mblur.rib) — Counter-rotating CSG gears with transformation motion blur (`Shutter 0 1`, `MotionBegin`).

### 3. Catmull-Clark Subdivision Surfaces (`SubdivisionMesh`)
* [`07_subdiv_creased_totem.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/07_subdiv_creased_totem.rib) — Showcases `catmull-clark` subdivision tags: variable sharpness `crease` edges, sharp `corner` vertices, `interpolateboundary`, and `hole` faces in a sculptural monolith.
* [`08_subdiv_organic_starfish_coral.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/08_subdiv_organic_starfish_coral.rib) — Complex branching genus-1 and multi-arm organic cages smoothed via Catmull-Clark subdivision with vertex-varying colors (`varying color Cs`).
* [`09_subdiv_retro_rocket_bay.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/09_subdiv_retro_rocket_bay.rib) — Hard-surface spacecraft hull combining smooth aerodynamic fairings with sharp creased panel seams and engine nozzles.
* **Camera/Time Variations:**
  * [`07_subdiv_creased_totem_dof.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/07_subdiv_creased_totem_dof.rib) — Shallow focus along a colonnade of subdivision totems.
  * [`08_subdiv_organic_starfish_coral_mblur.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/08_subdiv_organic_starfish_coral_mblur.rib) — **Deformation motion blur** (`MotionBegin` wrapping the `SubdivisionMesh` `"P"` vertex array) plus camera/object motion.

### 4. Implicit Blobby Surfaces (`Blobby`)
* [`10_blobby_lava_lamp_metaballs.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/10_blobby_lava_lamp_metaballs.rib) — Multi-ellipsoid `Blobby` (`1001` opcodes) with additive (`0`) blending and vertex colors (`varying color Cs`) forming stretching, merging liquid wax droplets.
* [`11_blobby_mercury_splash_crown.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/11_blobby_mercury_splash_crown.rib) — High-specular liquid metal splash using ellipsoid (`1001`), segment/sausage (`1002`), and subtractive (`2` / negative weight) opcodes to carve a central impact crater surrounded by a ring of droplets.
* [`12_blobby_molecular_cluster.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/12_blobby_molecular_cluster.rib) — Organic molecular bond structure using `1001` (ellipsoid atoms), `1002` (segment bonds), and `1003` (repelling ground plane) opcodes.
* **Camera/Time Variations:**
  * [`10_blobby_lava_lamp_metaballs_dof.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/10_blobby_lava_lamp_metaballs_dof.rib) — Macro depth of field through a field of floating metaball droplets.
  * [`11_blobby_mercury_splash_crown_mblur.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/11_blobby_mercury_splash_crown_mblur.rib) — High-velocity exploding droplets with `MotionBegin` trajectory blur.

### 5. Grand Showcase (All Features Combined)
* [`13_grand_showcase_atelier_dof_mblur.rib`](file:///Users/juvenal/Projects/RenderMan/scenes/13_grand_showcase_atelier_dof_mblur.rib) — Combines **Trimmed NURBS**, **CSG Solids**, **Creased Subdivision Surfaces**, **Blobby Metaballs**, **Depth of Field**, and **Motion Blur** in a single hero composition.

---

## Part 3: What Was Wrong, and What Was Changed

The scenes in this folder are copies of the originals in `~/Projects/RenderMan/scenes`, fixed for openRender. The links above still point at those originals. Every original rendered as a black (or nearly black) image. Three mistakes in the RIBs caused it, and each fix is the smallest change that restores what the scene describes: no geometry, camera, light position, intensity or shader parameter was changed.

### 1. Every light was switched off before any geometry was drawn (all 21 scenes)

Each scene declared its `LightSource`s inside an `AttributeBegin`/`AttributeEnd` block of their own:

```
AttributeBegin
    LightSource "ambientlight" 1 ...
    LightSource "pointlight" 2 ...
AttributeEnd
```

The list of active lights is an attribute (RISpec 3.2, `RiLightSource`), so `AttributeEnd` restores the empty list and every primitive after it is shaded with no lights at all. With the ambient light gone too, everything except `Surface "constant"` renders black.

**Fix:** the `AttributeBegin`/`AttributeEnd` pair around the lights is removed, so the lights are declared at world level and stay on until `WorldEnd`. The blocks held nothing but `LightSource` lines and comments, so nothing else changes. (The alternative, keeping the block and adding `Illuminate <n> 1` for each light after it, has the same effect.)

### 2. The trimmed NURBS kept only their holes (01, 01_dof, 02, 02_mblur, 03, 13)

Every `TrimCurve` starts with an outer boundary loop (loop 0) and then lists the holes, written in the IGES/OpenGL convention where the outer loop marks the region to keep. RenderMan's convention is different: a point's side is decided by how many loops enclose it (even-odd), and the default `Attribute "trimcurve" "sense" ["inside"]` cuts away what is enclosed. The outer loop encloses nearly the whole patch, so the patch was cut away and only the inside of each hole (enclosed twice) was left, which rendered as floating discs where the holes should be.

**Fix:** `Attribute "trimcurve" "sense" ["outside"]` before each `TrimCurve`, which keeps what an odd number of loops enclose: inside the boundary and outside the holes. Loop data is untouched. In 02 the nested island inside the centre hole (enclosed three times) is kept, as its comments intend.

### 3. CSG leaves leaked their transforms and colours into the next leaves (04, 04_dof, 05, 05_mblur, 06, 13)

In openRender, `SolidBegin`/`SolidEnd` does not save or restore the graphics state (whether other renderers scope a solid's leaves was not checked). A transform or `Color` inside a `SolidBegin "primitive"` block therefore carries into every leaf after it:

- the tooth bars of 05's gears each run `Rotate` and `Scale 1.62 0.22 0.22`, so each bar was scaled again on top of the previous ones, and the gears turned into large dark spheres;
- each closed cylinder flips its bottom cap with `Rotate 180 1 0 0`, which also flipped every leaf after it;
- the `Color` that marks a cut face (a bore, a window) recoloured every later leaf.

**Fix:** the body of every `SolidBegin "primitive"` block that changes the transform or an attribute is wrapped in `AttributeBegin`/`AttributeEnd`, so each leaf's transform and colour apply to that leaf only. (This is the same pattern openRender's own CSG test scenes use with `TransformBegin`; `AttributeBegin` is used here because the leaves also set `Color`.)

### Known renderer issue: CSG solids under motion blur (05_mblur, 13)

Tracked as GitHub issue #193.

`05_csg_planetary_gear_chronometer_mblur.rib` and `13_grand_showcase_atelier_dof_mblur.rib` have every fix above, but they do not render correctly yet. openRender currently renders any CSG solid wrong when it has transform motion blur (`MotionBegin` around its transform with `Shutter 0 1`): the solid is drawn away from its place as a faint smear or disappears, and with the Reyes hiders some solids also fill the log with `Too many eye splits` warnings (71,558 for 05_mblur, all from its balance wheel). This happens even for a single `SolidBegin "primitive"` around one torus. The same solids render correctly without motion, as do primitives with motion outside a solid. The RIBs are kept as they are so that they render correctly once the renderer is fixed. In 05_mblur all three moving solids (the two gears and the balance wheel) are affected. In 13 the spinning CSG chronometer cube at the centre of the frame is affected, while the canopy, the monolith, the Blobby plume and the depth of field render correctly.

### Not changed: dark `metal` surfaces (05, 07, 11, 03's copper wing)

The standard `metal` shader has no diffuse term, so it shows only specular highlights of the point lights and reflects nothing else; with no environment map or ray-traced reflections those surfaces are black except for their highlights (07's right-hand canopy looks like a black block, 11's mercury and 05's gears are dark). This is the scene's shading choice rather than an error, and is left as it was.
