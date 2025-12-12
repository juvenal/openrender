---
title: "Hiders"
date: 2025-12-08
---

# Hiders

Pixie can generate images using raytracing or scan line rendering. As it is often the case, scan line rendering is much faster than raytracing. However, such methods may not support accurate reflections/shadows. Since Pixie supports both types of algorithms, you can combine the best of the two worlds: speed of scan-line rendering and the accuracy of raytracing. A hider is the section of the renderer that is responsible for creating the final image. Essentially, every hider implements a different rendering algorithm. You can switch between defined hiders using  Hider command. Pixie defines the following hiders:

| Hider | Description |
|-------|-------------|
| **raytrace** | As the name implies, this hider creates the final image using raytracing. This involves shooting bunch of rays for every pixel (defined by  PixelSamples) and then filtering and summing the color of every ray (defined by `PixelFilter`). |
| **stochastic (hidden)** | (Default) This hider creates the final image using scan-line techniques very similar to Pixar's Reyes architecture. Every specified primitive is split into smaller primitives and deferred until needed. If the projected size of a subdivided primitive is small enough, a regular grid is sampled on the patch and the polygons in this grid are rendered using scan-line methods. Notice that a raytracer may need to keep the entire scene geometry in the memory in case a future ray can intersect them. On the other hand, the deferred and render-and-forget feature of this rendering algorithm allows it to keep a very small memory footprint |
| **zbuffer** | This hider is a stripped down version of Stochastic. It does not support motion blur, depth of field or transparency. If your scene does not involve these effects, this hider can generate an equal quality output with the Stochastic. *Note: This hider is recommended for simpler scenes for better performance.* |
| **photon** | This hider is used to compute photonmaps in a preprocessing step. This hider does not create an image. The renderer simply goes through the light sources defined in a scene and shoots photons.  \
**New in 1.7.2** The photon hider supports dispersion of light. |

The following table summarizes the capabilities of each hider:

| Hider | TSM | J | T | OC | AOS | SS | MB | DOF | LOD |
|-------|-----|---|---|----|-----|----|----|-----|-----|
| stochastic | X | X | X | X | X | X | X | X | X |
| raytrace |  | X | X |  | X | X | X | X | X |
| zbuffer |  |  |  | X |  | X |  |  |  |
| photon |  |  |  |  |  |  |  |  |  |

Where:

- TSM - Transparency Shadow Map
- J - Jittering
- T - Transparency
- OC - Occlusion Culling
- AOS - Arbitrary Output Samples
- SS - Super Sampling
- MB - Motion Blur
- DOF - Depth Of Field
- LOD - Level Of Detail