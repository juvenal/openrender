---
title: "FAQ: What's Wrong With My Render"
date: 2025-12-10
---

# FAQ: What's Wrong With My Render

## Displacements from textures

If you use a texture-map to displace the surface, you may find that grid cracks appear - small pinholes in the surface. This happens because the filter size chosen for adjacent grids may differ slightly, resulting in slightly different displacements which pull the surface edges apart.

Eg.

![Texture displacement causing patch cracks](/pixie-code/static/images/TexDisplacePatchCrack.jpg)

**What can you do?**

Turn on binary dicing, using

`Attribute "dice" "binary" 1`

Turn off the filtering on the texture lookup, by replacing your `texture(name,s,t)` with the following code:

```
   #define NUMPIXELS 1024
   #define SINGLEPIX (1/NUMPIXELS)
   P += amt*texture(texname,s,t, s+ SINGLEPIX,t, s,t+ SINGLEPIX, s+ SINGLEPIX,t+ SINGLEPIX)*normalize(N);
```

Additionally, you should try increasing the number of texture samples, ie.

```
   #define NUMPIXELS 1024
   #define SINGLEPIX (1/NUMPIXELS)
   P += amt*texture(texname,s,t, s+ SINGLEPIX,t, s,t+ SINGLEPIX, s+ SINGLEPIX,t+ SINGLEPIX,"samples",16)*normalize(N);
```