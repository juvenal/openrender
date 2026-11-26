---
title: "Closures, Bxdf and closuremode"
date: 2026-11-26
---

# Closures, Bxdf and closuremode

A **closure** describes what a point of a surface does with light (scatters it, emits it, lets it through) as data, not as a colour. A
shading point's closures form a small **tree**: components with weights, added and multiplied. openRender can build that tree for every
shaded point and hand it to a renderer that wants it (a path tracer, a light-transport integrator). This page covers the two statements a
scene uses: `closuremode`, which chooses whether the trees are built, and `Bxdf`, which binds a built-in material without a shader.

A scene that uses neither statement renders exactly as before.

## closuremode

```rib
Option "shade" "string closuremode" [ "both" ]            # the whole scene
Attribute "shade" "string closuremode" [ "closures" ]     # one object (the attribute wins over the option)
```

| Mode | What the surface step does |
|---|---|
| `color` | The default. The surface shader runs as it always did and no tree is built. |
| `both` | The surface builds a tree for each point and computes its colour too; the image is the same as `color`'s. |
| `closures` | The surface builds the tree, no light is evaluated and the colour is 0; the opacity the shader wrote is kept. |

The attribute is saved and restored by `AttributeBegin`/`AttributeEnd`, `SolidBegin` and `ObjectBegin`, like any attribute; an object
instance takes the mode in force at `ObjectInstance`.

The shaders shipped with openRender do not build closures yet, so under `both` or `closures` they render exactly as in `color`, and
openRender says so once per shader and mode in each frame. Trees are delivered to a consumer, which a renderer installs; with none
installed `both` is `color`, and `closures` writes colour 0 with a warning.

## Bxdf

```rib
WorldBegin
    AttributeBegin
        Color [ 0.8 0.3 0.2 ]
        Bxdf "plastic" "float roughness" [ 0.2 ]
        Sphere 1 -1 1 360
    AttributeEnd
WorldEnd
```

`Bxdf "name" ["handle"] <parameters>` binds a built-in material in the current attribute block. Of `Surface` and `Bxdf`, the one bound last
governs the block, and `AttributeEnd` brings back the enclosing choice. The base colour is the current `Color` and the opacity the current
`Opacity`.

| Bxdf | Parameters (default) |
|---|---|
| `diffuse` | `Kd` (1); `roughness` (0): above 0 the surface is rough diffuse (Oren-Nayar) |
| `plastic` | `Kd` (.5), `Ks` (.5), `roughness` (.1), `specularcolor` (1); `ior` (0: no Fresnel term; above 0: dielectric Fresnel) |
| `conductor` | `Ks` (1), `roughness` (.1) |
| `dielectric` | `ior` (1.5), `roughness` (0), `reflectColor` (1), `transmitColor` (1) |

`roughness` is the GGX `alpha` itself, not its square. A value out of range is clamped with a message naming the parameter; a negative
`plastic` `ior` or a `dielectric` `ior` of 0 or less refuses the statement. `Ka` is not a parameter.

Rules to know:

- A `Bxdf` is **not valid inside `ObjectBegin`/`ObjectEnd`**; bind it at the instance (`ObjectInstance` takes the binding in force there).
- A name starting with `sl:` or `osl:` is reserved for later shading-language Bxdfs.
- Without a handle, a first parameter whose value is a bare string must be written in brackets, or it is read as the handle.
- The statement does not change the random numbers of any other shader.
- Until an integrator that consumes closures is installed, an object governed by a `Bxdf` renders with the default surface (the surface
  `WorldBegin` binds; plain `Cs` times `|I.N|` if a `Surface` was given before `WorldBegin`), with one warning per Bxdf name per frame. It
  never uses a `Surface` bound in the same block.

The grammar is in the RIB guide (`DEVNOTES_DETAILS/RIB_GUIDE.md`) and the internals in the closure model guide (`DEVNOTES_DETAILS/CLOSURES.md`).
