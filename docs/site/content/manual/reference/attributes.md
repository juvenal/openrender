---
title: "Attributes"
date: 2025-12-10
---

# Attributes

## Dice attributes

```
Attribute "dice" "int numprobes[2]" [3 3]
```

During the scan line rendering, the renderer needs to estimate the bounding box of a piece of a primitive. Pixie does this by sampling points on the surface and then extending the bounding of these points. "numprobes" controls the number of samples in u and v directions to take in order to estimate the bound. Notice that this is just an estimate and the renderer may underestimate the correct bound. The right way to do this is to actually subdivide the surface. But this consumes lost of computation and memory.

```
Attribute "dice" "int minsplits" [2]
```

This controls the minimum number of times a surface is split before dropping into the reyes pipeline. Since the pixie estimates the surface bounds by sampling, this number must be greater than 0 for some primitives.

```
Attribute "dice" "float boundexpand" [0.5]
```

This is the factor by which Pixie will overestimate the bound of the surface pieces. The bounding box computed by point sampling on the surface will be expanded by this factor.

```
Attribute "dice" "int binary" [0]
```

If this value is 1, when sampling a grid on a surface piece, create a power of two edges on the boundaries. This attribute is used to help the patch cracking problem.

## Displacement attributes

These attributes are used to tell renderer how much a displacement shader actually displaces the surface. This information is vital for accurately tessellating and rendering surfaces displaced surfaces.

```
Attribute "displacementbound" "float sphere" [0]
```

This is the amount in the displacement coordinate system by which the displacement shader can move the surface.

```
Attribute "displacementbound" "string coordinatesystem" "current"
```

This command sets the coordinate system that the displacement bound is expressed in.

## Visibility attributes

**Note:** In 2.2.1 the `"transmission"` visibility attribute has changed, and the string form is deprecated. The `"trace"` visibility attribute is also superceded by `"specular"` and `"diffuse"` visibility modes.

The attributes in this class control the visibility behaviors of objects.

```
Attribute "visibility" "int camera" [1]
```

If 1, the object is visible to camera rays (and to the scanline renderer).

```
Attribute "visibility" "int specular" [0]
```

If 1, the object is visible to rays created by the `trace()` shading language command.

```
Attribute "visibility" "int diffuse" [0]
```

If 1, the object is visible to rays created by the `indirectdiffuse()` and `occlusion()` shading language commands.

```
Attribute "visibility" "int photon" [0]
```

If 1, the object is visible to photons (i.e. it will block / bounce photons).

```
Attribute "visibility" "int transmission" [0]
```

Specifies visibility to transmission rays (those fired by `shadow()` and `transmission`).

<div class="pre">`Attribute "visibility" "int trace" [0]`</div>
<div class="notebox" style="color: #ff0000">Deprecated in 2.2.1 </div>

If 1, the object is visible to rays created by the trace shading language command.

<div class="pre">`Attribute "visibility" "string transmission" "transparent"`</div>
<div class="notebox" style="color: #ff0000">Deprecated in 2.2.1 </div>

This value controls the shadowing behavior of the object. If this value is "transparent", the object is not visible to the transmission rays and is considered completely transparent. If it is "opaque", the object is considered opaque. If it is "shader", the surface shader is executed to find out the opacity. If it is "Os", the opacity of the surface is copied from the Os attribute. **This string form of the attrbute is deprecated. Please use the int version (specifying int explicitly) like:**

```
Attribute "visibility" "int transmission" [1]
```

## shade attributes

These attributes control when shading will occur

```
Attribute "shade" "string transmissionhitmode" ["primitive"]
```

Control whether opacity for transmisison rays is determined by the surface shader (`"shader"`), or the `Opacity` atribute (`"primitive"`).

```
Attribute "shade" "string diffusehitmode" ["primitive"]
```

Control whether opacity and color for diffuse rays are determined by the surface shader (`"shader"`), or the `Opacity` atribute (`"primitive"`). The `"shader"` mode is not currently supported,

```
Attribute "shade" "string specularhitmode" ["shader"]
```

Control whether opacity and color for specular rays are determined by the surface shader (`"shader"`), or the `Opacity` atribute (`"primitive"`).

```
Attribute "shade" "string camearhitmode" ["shader"]
```

If the camera hitmode is specified to be `"primitive"` then the `Opacity` attribute is used instead of the shader's calculated `Oi`. This allows you to force Pixie to treat the object as opaque (and therefore cull objects behind it).

## Trace attributes

These attributes control the raytracer behavior.

```
Attribute "trace" "int displacements" [0]
```

If 1, the surface will be displaced before the raytracing. True displacements for raytracing may be very expensive. Use this attribute sparingly as for most displacement shaders, this will not create noticeable change.

```
Attribute "trace" "float bias" [0.01]
```

This is the self-intersection bias. This single value is used for all raytracing as well as shadow mapping computations. Notice that the default value is different than the PrMan's shadow option.

```
Attribute "trace" "int maxdiffusedepth" [1]
```

This is the maximum number of diffuse photon bounces to compute with the photon hider.

```
Attribute "trace" "int maxspeculardepth" [2]
```

This is the maximum number of specular photon bounces to compute with the photon hider.

## Irradiance attributes

These attributes control the irradiance / occlusion caching.

```
Attribute "irradiance" "string handle" ""
```

This is the irradiance cache file. Irradiance caching is usually a two pass approach: in the first pass, an irradiance cache is computed and saved into the file whose name is given by this attribute. In the second pass the same file used. Notice that this is an attribute, meaning different objects can save irradiance / occlusion information into separate files. This file is used by both indirectdiffuse and occlusion shading language commands.

```
Attribute "irradiance" "string filemode" ""
```

This argument controls how the irradiance file is used. Valid argument values are "","r", "w" for no r/w, read and write. You want to use "w" for the first pass and "r" for the second.

```
Attribute "irradiance" "float maxerror" [0.5]
```

This is a quality knob for the irradiance cache. 0 means a new sample is computer for each shading point without caching (high quality, much slower).

```
Attribute "irradiance" "float maxpixeldist" [20]
```

This is a second quality knob for the irradiance cache. It specifies the maximum radius of validity for a cache sample - meaning interpolation cannot occur over a larger distance. This forces more samples into the cache (in a uniform manner, whereas maxerror is adaptive wrt. the change of irradiance / occlusion). The value is approximately in screen pixels.

## Photon attributes

These attributes control the photon mapping.

```
Attribute "photon" "string globalmap" ""
```

This attribute specifies the global photonmap to create if photon hider is being used.

```
Attribute "photon" "string causticmap" ""
```

This attribute specifies the caustic photonmap to create if photon hider is being used.

```
Attribute "photon" "string shadingmodel" "matte"
```

This attribute gives the shading model to use when scattering photons. The valid values are: "matte", "glass", "water", "chrome", "dielectic" and "transparent".

- `"matte"` - only matte bounces will be recorded (the surface does not scatter specular / caustic photons.
- `"glass"` - the ior is 1.5, photons may be specular reflections or transmitted refractions - both caustic
- `"water"` - the ior is 1.3, photons may be specular reflections or transmitted refractions - both caustic
- `"chrome"` - photons are reflected, only caustic photons stored
- `"dielectic"` - the ior is specified by `Attribute "photon" "ior" n`, reflections and refractions are stored - both caustic
- `"transparent"` - invisible to photons

```
Attribute "photon" "ior" 1.5
Attribute "photon" "iorrange" [1.5 1.4]
```

Specifies the index of refraction for the dielectric photon model. The `ior` attribute make the surface respond to all wavelengths of light with the same index of refracion. The `iorrange` attribute allows for variation in index of refracion based on wavelengh. The syntax is `[ior_for_red ior_for_blue]`. See [Tutorials/Dispersion](../../Tutorials/Dispersion.html) for details.

```
Attribute "photon" "int estimator" "100"
```

This is the number of photons to use when estimating the irradiance. Bigger numbers will cause smoother but blurrier estimates. The smaller numbers will create sharper but noisier image.