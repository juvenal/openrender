---
title: "Raytracing from C"
date: 2025-12-10
---

# Raytracing from C

You can use the Pixie runtime library to raytrace instead of creating an image. There are three functions that can be used just before `WorldEnd` to raytrace. Since these functions are "C" specific, they do not have rib bindings. Also these functions are not standard RenderMan (they are Pixie specific). If any of these functions are used, `WorldEnd` is skipped and no image will be generated. The auxiliary raytracing commands are:

```
void RiTrace(RtInt n,RtPoint *from,RtPoint *to, RtPoint *Ci);
```

This function traces n rays starting from "from" and going towards "to" and returns the radiance in "Ci". The parameters are arrays each of which has "n" items.

```
void RiVisibility(RtInt n, RtPoint *from, RtPoint *to, RtPoint *Oi);
```

This function is the same with `RiTrace` except that it returns the opacity between two points.

Note that these functions trace multiple rays. The performance is improved if the number of rays is high and if the rays have strong spatial coherence (i.e., they roughly start from the same position and go towards the same direction). Depending on the demand, I can add more raytracing functions that have more specialized parameters.