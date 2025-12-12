---
title: "Raytraced Shadows"
date: 2025-12-08
---

# Raytraced Shadows

In general, shadow mapped shadows are faster if generated correctly. The disadvantage of creating shadows with shadow mapping is the need for a separate shadow map generation.

To make sure an object casts shadows, you need to set the visibility attribute:

```
Attribute "visibility" "transmission" "opaque|Os|shader|transparent"
```

The default value of this attribute is `transparent`. This means by default no object will cast any shadows. opaque means the object will cast a completely opaque shadow. Os means the shadow opacity will be determined from the opacity attribute and finally, shader means the surface shader will be executed to find the shadow opacity. Notice that shader is naturally slower than the other alternatives that don't require shader execution.

In order to compute the amount of visibility between two points, you may use the transmission function, which returns the visibility between two points.

You can also use the shadow function within illuminate or solar constructs with "raytrace" as the shadow map name to figure out the visibility of the query point using raytracing.

The following RIB file demonstrates the raytraced shadows using the shadowdistant light source:

```
Display "shadow.tif" "framebuffer" "rgb"
Projection "perspective" "fov" 45
PixelSamples 2 2
PixelFilter "catmull-rom" 3 3
Translate 0 0 10
Rotate 80 1 0 0
Rotate 80 0 0 1
WorldBegin
LightSource "shadowdistant" 1 "from" [0 0 0] "to" [0 0 1] "shadowname" "raytrace" "intensity" 0.6
LightSource "ambientlight" 2 "intensity" 0.2
Surface "matte"
Polygon "P" [-5 -5 1 5 -5 1 5 5 1 -5 5 1]
Translate 0 0 -1
Rotate 40 -1 -1 -1
Scale 2 2 2
Attribute "visibility" "transmission" "opaque"
Polygon "P" [-1 -1 0 1 -1 0 1 1 0 -1 1 0]
WorldEnd
```

It generates the following image:

![Raytraced shadow example showing a polygon casting shadow using shadowdistant light source with 'raytrace' shadow name](/pixie-code/static/images/Shadow1.jpg)

Notice that shadowdistant uses the shadow call with "raytrace" as the shadow name. If you change:

```
Attribute "visibility" "transmission" "opaque"
Polygon "P" [-1 -1 0 1 -1 0 1 1 0 -1 1 0]
```

to

```
Attribute "visibility" "transmission" "shader"
Polygon "P" [-1 -1 0 1 -1 0 1 1 0 -1 1 0] "Os" [1 0 0 0 1 0 1 1 0 0 0 1]
```

You should get the following picture:

![Raytraced shadow example with shader-based transmission showing transparency effects](/pixie-code/static/images/Shadow2.jpg)