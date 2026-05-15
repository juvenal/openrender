# Contract: Imager Shader Variables

**Scope**: Built-in variables available inside an `imager` shader declaration.

---

## Shader Declaration

```sl
imager shadername (
    /* user-defined parameters */
)
{
    /* body: read/write Ci, Oi, alpha */
}
```

## Built-in Variables

| Variable | Type | Access | Description |
|----------|------|--------|-------------|
| `Ci` | `color` | read/write | Current pixel color in linear floating-point space. Modified value is sent to all active display drivers. |
| `Oi` | `color` | read/write | Current pixel opacity. Synthesized from `alpha` on entry; written value is folded back into `alpha` on exit. |
| `alpha` | `float` | read/write | Pixel coverage alpha (0 = fully transparent, 1 = fully opaque). |
| `P` | `point` | read-only | Raster-space position: `(x + 0.5, y + 0.5, 0)` where `(x, y)` is the pixel's integer column/row in output image space. |
| `ncomps` | `float` | read-only | Number of color components (3 for RGB). |
| `time` | `float` | read-only | Shutter open time for this frame. |
| `dtime` | `float` | read-only | Duration of the shutter interval (`shutterClose - shutterOpen`). |

## Execution Context

- The imager executes **once per output pixel per frame**, after geometric shading, pixel filtering, and sample compositing are complete.
- Input values are **linear floating-point** (no gamma correction, no quantization applied yet).
- The imager operates on the **filtered** (reconstructed) pixel value, not on individual sub-pixel samples.
- Execution order relative to display output: `imager → gamma/quantize → display drivers`.

## Constraints

- Standard surface built-in functions (`illuminate()`, `trace()`, `texture()`, etc.) are **not available** inside an imager shader.
- The imager cannot access per-object surface attributes or light sources.
- Writing to read-only variables (`P`, `ncomps`, `time`, `dtime`) has no effect on the output.

## Example — Background Fill

```sl
imager background(
    color bgcolor = 1;
    float background = 1;
)
{
    Ci += (1 - alpha) * (bgcolor * background);
    Oi =  1;
}
```

## Example — Screen-Space Vignette

```sl
imager vignette(
    float radius = 0.8;
    float softness = 0.5;
)
{
    float cx = xresolution / 2;
    float cy = yresolution / 2;
    float dist = sqrt((P[0]-cx)*(P[0]-cx) + (P[1]-cy)*(P[1]-cy));
    float r = sqrt(cx*cx + cy*cy);
    float t = smoothstep(radius * r, (radius + softness) * r, dist);
    Ci *= (1 - t);
}
```
