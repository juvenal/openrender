---
title: "Baking 3D Textures"
date: 2025-12-10
---

# Baking 3D Textures

## Introduction

There are times when a shader calculation is expensive, but predictable, and you'd like to be able to cache the result from one execution and reuse it. You don't just have to cache the whole shader result - you could cache patterning on a surface, and recombine it with lighting later.

Pixie now supports baking arbitrary data to 3d textures. You don't need to provide a uv texture mapping for this to work - so it's a rather powerful and general data storage mechanism.

The outline operation is:
- call `bake3d()` from your shader to save a point cloud of your data
- convert the point cloud to a 3d bricked texture using `texmake`
- read the texture using `texture3d`

### Point Clouds

A point cloud is a structure which can hold arbitrary attributes / data produced by the `bake3d` shadeop. It's construction is straightforward, but it's format requires that the whole cloud is held in memory - this means that reading them can be costly, even if only a small fraction of the data is accessed.. Whilst point clouds support 'blurred' lookups of sorts, they do not really support level of detail - efficient lookups which represent all the data when viewed far away.

### 3d Textures

Pixie supports a 3d Texture format which can be prepared from a point cloud. Once prepared, the 3d texture supports efficient lookups, which support level-of-detail. The entire texture does not have to be held in memory, but instead is paged in as needed. Whilst the preparation of the texture can be a little time consuming, it is usually done offline, and reused for multiple frames. If the data baked into the texture is expensive to compute, this can represent quite a saving.

Both point clouds and 3d textures can be read by the `texture3d` shadeop.

## Bake3d

Baking 3d data to a 'point cloud' is done via the following shadeop

```
bake3d(string filename,string channels,point P,normal N,...,<data>);
```

### `channels`

You must predeclare each channel to be baked using the following syntax in your RIB

```
DisplayChannel "type name";
```

eg

```
DisplayChannel "color CoutDiff";
```

The channels are then passed to `bake3d` in the `channels` parameter, multiple channels are passed as a single string, with commas to separate them:

```
"CoutDiff"
```

or

```
"CoutDiff,CoutSpec"
```

for multiple channels.

### `P,N`

The `P` parameter specifies the point at which the data is given. The `N` parameter allows you to specifiy a normal for surface data, allowing nearby surfaces to be differentiated. If the data you are baking is volume data (has no valid N), then pass `N=0`.

### optional parameters

The optional parameters are:

- `radius` - Specifies the validity radius of each sample. If you do not provide this, Pixie will calculate a value for you using the grid size.
- `radiusscale` - Allows a scale to be applied to the calculated (or provided) radius
- `interpolate` - if 1, then bake the centres of micropolygons, rather than the corners - this will produce fewer overlapping points at the borders between grids, and should be used if the point cloud is to be processed with a program for which overlapping points might be an issue.

Data to be baked is then specified with name-value pairs, eg

```
 "CoutDiff",Cd
```

so the full call might look like:

```
color Cd = <something complex>;
bake3d("cloud.ptc","CoutDiff",P,N,"CoutDiff",Cd);
```

## Using `texmake` to prepare textures

To prepare these point clouds into a friendly format for reading, use the `texmake` program. The `-texture3d` option to `texmake` specifies that you wish to prepare a 3d texture. The syntax is:

```
texmake -texture3d [-maxerror n.n] <input point cloud> <output 3d texture>
```

The `-maxerror` parameter allows you to tweak how compression of the texture will occur. The texture is stored in an octree-like format, with each level of the map representing a coarser approximation to previous levels. For very uniform data, there is no need to store the finest representations as coarser levels will represent the data perfectly well. The `-maxerror` parameter represents the maximum variation for which data will be considered uniform.

By setting maxerror (default 0.002) you can control the size/space tradeoff for the map. Setting this number too large will result in 'blocky' lookups, and too small will hurt performance with no gain in quality.

For example, the following command line might be used to prepare the baked cloud to a 3d texture

```
texmake -texture3d cloud.ptc surftexture.bm
```

of perhaps after some experimentation:

```
texmake -texture3d -maxerror 0.004 cloud.ptc surftexture.bm
```

## Alternative for preparing a texture

You may also put the following fragment in your RIB **outside** WorldBegin / WorldEnd:

```
MakeTexture3D <inputfile> <outputfile>
```

or

```
MakeTexture3D <inputfile> <outputfile> "maxerror" [float]
```

## texture3d

Reading back a 3d texture or point cloud is done via the following shadeop:

```
texture3d(string filename,point P,normal N,...,<data>);
```

### `P,N`

The `P` parameter specifies the point at which data should be looked up. The `N` parameter allows you to specifiy a normal for surface data, allowing nearby surfaces to be differentiated. If the data you are lookup up is volume data (has no valid N), then pass `N=0`.

### optional parameters

The optional parameters are:

- `radius` - Specifies the sampling radius of each sample. If you do not provide this, Pixie will calculate a value for you using the grid size.
- `radiusscale` - Allows a scale to be applied to the calculated (or provided) radius - this allows the lookup to be blurred.

The data stored in the file is read into variables supplied to `texture3d` with name-value pairs, eg

```
 "CoutDiff",Cd
```

Unlike when baking, no channels have to be supplied, and the data's names are checked against those stored in the file.

so the full call might look like:

```
color Cd = 0;
texture3d("cloud.ptc",P,N,"CoutDiff",Cd);
<use Cd>
```

## Reference Positions

If the object you're baking is moving or deforming, you might wish to bake the data onto a reference pose, and look it up using that pose (using a vertex point __Pref to specify the vertices in reference pose). Future releases may support baking to an arbitrary coordinate system other than 'world'.