# Render architecture references: integrators, closures, RSL 2.0, OSL

Reference material for the `speckit.plan` and `speckit.implement` stages of the
integrator / closure / RSL 2.0 / OSL specs (the "A/B tracks" of the Principle
VIII plan). It collects:

- the **binding decisions** taken with the maintainer (2026-10-13 → 2026-10-15),
  each with its source;
- the **code samples, interfaces and definitions** from two design
  conversations held outside this repo, cleaned up, each tagged with what we do
  with it;
- **corrections** to those conversations, checked against primary sources.

Nothing here is an API yet. Sketches are conceptual C++ unless a section says
otherwise; the frozen boundaries are C function tables (§9).

**Sources.**
- **Chat 1**, "OSL Raster Integration Feasibility" (2026-10-03 → 10-13): OSL in a
  raster/REYES renderer, OSL's light model, RSL/OSL coexistence, JIT evolution.
- **Chat 2**, "Design RSL Integrator API" (2026-10-13): closure-producing RSL,
  a standard integrator API, a raster integrator, BDPT requirements. It arrived
  in three pieces that **overlap**: piece 1 ran to §4.4 (cut mid-definition),
  piece 2 ran §3–§11.4, piece 3 restarted at §9 with different content. Where
  two versions of the same section disagree, both are cited as `2a`/`2b`.
- **OSL sources** checked 2026-10-15: `src/shaders/stdosl.h` (standard closure
  signatures), `src/include/OSL/oslclosure.h` (closure tree layout),
  `INSTALL.md` (dependency floors).

**Tags.**

| Tag | Meaning |
|---|---|
| **ADOPT** | Take as specified (names may change to house style). |
| **ADAPT** | Take the idea; the shape changes as described. |
| **RESERVE** | Design room for it now (a field, a flag, a convention); implement later. |
| **DEFER** | Good idea, own spec later; nothing to do now. |
| **REJECT** | Not taken, with the reason. |
| **FIX** | The source was wrong or garbled; the corrected form is given. |

---

## 0. Binding decisions

These override anything the conversations say. "Answer N" refers to the
maintainer's numbered answers in the planning session; "advised" marks items
the maintainer asked advice on and did not explicitly confirm (re-confirm at
`specify`).

| # | Decision | Source |
|---|---|---|
| D1 | **Hider = visibility** (reyes, zbuffer, raytrace, photon); **integrator = transport** (classic, direct, pathtracer, later bdpt). Orthogonal choices. | discussion, accepted |
| D2 | `Integrator "classic"` = today's behaviour: the `Surface` shader integrates itself (standard RSL, no closures). It is the byte-identical **default** and exists from the moment the `Integrator` abstraction exists. | answer 6 + "no drop-offs" |
| D3 | `Integrator "direct"` is implemented **first**, shading **native C++ Bxdfs** (RIS-style). | answers 1 (round 3), 1 (round 4) |
| D4 | Path tracing = `Hider "raytrace"` + `Integrator "pathtracer"`. `Hider "pathtrace"` is a registered shorthand for both (spelling: `"pathtrace"`). | answers 1 (round 2), 5 |
| D5 | Within an attribute scope, the **last** of `Surface` / `Bxdf` wins; both are saved/restored by `AttributeBegin/End`; ribOut must keep statement order. | answer 2 (round 3) |
| D6 | `Surface`-only objects under a non-classic integrator: **b** (Ci as emission) by default; **d** (hybrid) and **a** (error) selectable by option/attribute. Under `direct` there are no secondary bounces, so b and d render alike there. | answer 2 (round 4); extension to `direct` confirmed 2026-10-15 |
| D7 | Every plug-in type is a **semver-versioned** plug-in: Bxdfs, integrators, lights, light filters, display filters, sample filters, patterns, hiders. The boundary is a **C function table** plus a C++ convenience header. | round 5 answer 1, round 7 answer 3 |
| D8 | Built-in registries come **first** (same versioned interfaces); **run-time loading** arrives with the "RIS plug-in types" spec after 037. Applies to integrators, Bxdfs and every other plug-in type. | answers 12, 4 (round 7) |
| D9 | `Bxdf "name"` resolution: plug-in registry → `.slo`/`.rslo` (existing `shaderformat` precedence picks between them) → `.oso`. Prefixes `"sl:name"` / `"osl:name"` force a language. `Surface` stays RSL-only and never searches plug-ins or `.oso`. | answer 8 |
| D10 | Lights come in **three kinds**: C++ light plug-ins, RSL 2.0 physically-plausible lights, classic `LightSource` shaders through an adapter. | answer 7 |
| D11 | **RSL 2.0** = Pixar's RSL 2.0 (superset of RSL), full scope in 5 layers, each on the interpreter then the JIT: (1) structs + resizable arrays, (2) class shaders + shading pipeline, (3) co-shaders, (4) physically plausible shading, (5) closures. | rounds 2–4 |
| D12 | RSL 2.0 keeps the **`.sl`** extension (detected from syntax; optional `-std=`); `.rslo`/`.slo` carry a language-level stamp. OSL uses `.osl` → `.oso`. | answer 1 (round 6) |
| D13 | Closures in `.sl` are **allowed by default** (compiler). Independently, the runtime `closuremode` (`color`\|`both`\|`closures`, attribute > option > default `color`) keeps its original meaning. The two do not conflict: one is what the compiler accepts, the other what a `Surface` binding consumes. | answers 4 (round 5), 2 (round 6) |
| D14 | Closure **syntax is OSL's**: `closure color` (untyped), assigned to `Ci` inside a class-shader method; OSL standard closure names (§2.3). | round 7 answer 2 |
| D15 | A shader bound with `Bxdf` under any non-classic integrator gets a **bind-time error** if it calls `illuminance`, `directlighting`, `indirectdiffuse`, `indirectspecular`, `gather` or `occlusion`. `trace()` stays allowed as a query (OSL parity). Layer-4 `generateSamples`/`evaluateSamples` methods are **not** affected: they describe scattering and are one of the BSDF implementations behind the integrator. | round 8 answer 1 |
| D16 | **Presence** of a `Bxdf` shader = weight of the `transparent()` closure inside `Ci` (OSL / Arnold / Cycles convention). `Oi` stays compositing opacity; legacy `Surface` keeps colour-`Oi` behaviour. | round 8 answer 2 |
| D17 | **Closure storage** mirrors OSL's tree layout (component id + weight + parameter block; add and multiply nodes; arena-allocated, POD), built in-tree in 035 **without** linking OSL; the OSL backend spec adds a layout/zero-copy compatibility test against `oslclosure.h`. A flattened lobe list is the fast path for purely additive trees; a generic evaluator handles the rest. | round 8 answer 3 |
| D18 | **Colour abstraction**: new interfaces use one `Spectrum` value type with a compile-time channel count (3 now), never `.r/.g/.b` outside conversions; at the C ABI an array + channel count in the descriptor. RSL `color` unchanged. Exception: closure-tree *weights* stay 3-channel to keep OSL zero-copy (D17); evaluation code still uses `Spectrum`, and going spectral would change the tree layout. | answer 19; weights confirmed 2026-10-15 |
| D19 | **Shared BRDF math library** (microfacet D/G, Fresnel, lobe sampling + pdfs, frames, MIS weights) in the Shading domain, first used by 035's built-in Bxdfs, then by RSL 2.0 and OSL closures. Classic `diffuse()`/`specular()`/`phong()` builtins are **not** rerouted (byte-identical). Written from the papers; any adapted outside code gets an LGPL-2.1 licence check. | answer 17, confirmed 2026-10-15 |
| D20 | **Shading rate means the same in every hider**: `ShadingRate` (area in pixels, per-object attribute) = distinct shading evaluations per pixel area (0.25 ≈ 4 per pixel; 4 ≈ one per 2×2 pixels); `PixelSamples` = visibility only. No separate frequency switch. Raytrace shades camera hits on REYES-diced grids and interpolates at the hit (u,v); filter footprint = √ShadingRate pixels everywhere; secondary rays per hit. Spec 054, before 042; opt-in, default flipped in its last phase. **Open:** material-per-cell vs lighting-per-sample under Bxdf integrators. | answer 18 (round 9–10), confirmed 2026-10-15 |
| D21 | Integrators negotiate **versioned capabilities** (required / optional / provided). | discussion, accepted |
| D22 | Three versions move independently: **RSL language**, **closure ABI**, **renderer-services API**. | chat 2b §14, accepted with D7 |
| D23 | **OIIO**: ≥ 3.0, vendored. Arrives as an **extra** image backend (decode + one display driver + texmake writing); the direct libtiff/libpng/OpenEXR code, the in-tree RGBE codec, the JIT-off build and the CMake 3.19 floor are removed **when OSL lands**. OIIO is linked into exactly one shared library. | answers 3, 14, round 7 answer 5 |
| D24 | **OSL is integral** once it lands (no option), making LLVM (floor 18, no ceiling; vcpkg stays on 23 + `clang` feature) and OIIO integral. CMake floor → 3.23. The Linux FHS package **bundles** OIIO 3 and OSL (+ OCIO if 24.04's is too old) and keeps 24.04's stock LLVM 18. | answers 3–4 (round 4–7), round 7 answer 6 |
| D25 | One compiler library (versioned C API + C++ wrapper, shared + static) behind `oshader`, which emits `.rslo`, `.slo` and `.oso`. A public **runtime** library (C API + C++ wrapper over `ShadingSystem` + `RendererServices`) for editor previews; spec 026's interfaces stay internal C++ and get the public facade later. | answers 4–6 (rounds 4–5), 13, 15 |
| D26 | Global illumination with Bxdf materials (photon scattering, irradiance-cache `indirectdiffuse`) is its own spec **after 032 and 040**; until then those cells are explicit diagnostics. | answer 11 |
| D27 | Native JIT lowering is a **deferred** spec after 033 and the GitHub #8 fix. | answer 16 |
| D28 | No drop-offs: every hider × integrator × {`Surface`, `Bxdf`} cell either renders or fails with an explicit diagnostic. | round 2 "additional info" |
| D29 | The OSL hands-on spike (minimum dependency set, measured footprint) runs right before the OSL backend. | answer 6 (round 3) |
| D30 | **Principle VIII domain placement** (constitution amendments, each landing with the first spec that needs it): (1) a tenth domain, **Transport**, holds the integrators; it may depend on Graphics State, Geometry and Shading, and Hiders may depend on it (030). (2) The light service, BRDF math library, closure registry and the public compiler/runtime libraries live in **Shading** (no amendment). (3) Rule 4's versioned C plug-in boundary extends from the three named families to **every** plug-in type; 030/035/041 already shape their built-in interfaces that way, and the amendment lands with 051, when they become external extension points. (4) The shared OIIO wrapper is a **utility library** beside `libRIBCommon`, because Texturing and Display Outputs may not depend on each other (048). | confirmed 2026-10-15 |

---

## 1. Architecture axes

### 1.1 Three independent choices — ADAPT

Chat 1 §"three independent choices" and chat 2 §1:

```text
Visibility (hider)      Shading (language × backend)       Transport (integrator)
  reyes                   RSL   × interpreter (.rslo)        classic   (shader integrates)
  zbuffer                 RSL   × LLVM JIT    (.slo)         direct    (renderer integrates, no bounces)
  raytrace                OSL   × OSL JIT     (.oso)         pathtracer
  photon (GI pass)        C++ Bxdf plug-ins                  bdpt (later)
                    ───────── shared renderer services ─────────
       textures · attributes · transforms · derivatives · ray queries · light sampling
```

Adaptation: chat 2's "raster integrator" owns rasterization itself. Here it is
**our reyes/zbuffer hider + `Integrator "direct"`**: hiders produce shading
points, integrators never rasterize, so no `IRasterService` is exposed to
integrators (§5.4).

### 1.2 Compatibility matrix — ADOPT (as the D28 test suite)

Chat 1 §7, extended with the decisions:

| Material \ cell | reyes/zbuffer + classic | reyes/zbuffer + direct | raytrace + direct | raytrace + pathtracer | photon hider |
|---|---|---|---|---|---|
| `Surface` (RSL) | native (today) | D6 b/d/a (b = d here) | D6 | D6 | today's illumination hook |
| `Bxdf` C++ plug-in | *clarify in 042* | **042 milestone** | 042 | 050 | diagnostic until bxdf-gi (D26) |
| `Bxdf` RSL 2.0 (layer 4 methods / layer 5 closures) | *clarify* | after layer 4/5 | after layer 4/5 | after 050 | diagnostic until bxdf-gi |
| `Bxdf` OSL | *clarify* | after 049 | after 049 | 049 + 050 | diagnostic until bxdf-gi |

"Supported" means architecturally achievable after the named spec, not free.

### 1.3 What we do not promise — ADOPT

Chat 2b §17: material semantics are shared across integrators; **images are
not**. A direct integrator cannot reproduce global transport because it
evaluates the same closures. Tests compare like with like (§6.5).

---

## 2. Materials and closures

### 2.1 The core idea — ADOPT

Chat 1 §1: *the shader describes scattering; the integrator decides how
scattering becomes pixels.* A closure is a symbolic description; closures
**never spawn rays** (chat 1 "one thing I would not do"): the integrator owns
sampling and ray budgets.

```text
rasterization / ray hit
     │  P, N, Ng, uv, dPdu/dPdv, I, attributes
     ▼
execute shader group  ──►  closure tree  ──►  integrator interprets it
                                             ├─ diffuse    → evaluate against lights
                                             ├─ microfacet → direct specular
                                             ├─ emission   → add emission
                                             ├─ transparent→ presence (D16)
                                             └─ …
```

### 2.2 Classic RSL vs closure semantics — NOTE (why `Surface` ≠ `Bxdf`)

Chat 1 §2. A classic shader's `Ci` is *already-lit radiance*; nothing can
recover a BSDF from it:

```c
/* classic RSL: Ci is final, lit colour */
surface plastic_shader(color baseColor = 1; float roughness = 0.2;)
{
    normal Nf = faceforward(normalize(N), I);
    Ci = baseColor * diffuse(Nf) + specular(Nf, -normalize(I), roughness);
    Oi = Os;
}
```

| Property | Classic RSL | OSL / closure RSL |
|---|---|---|
| Material execution | often computes lighting | describes scattering |
| Shader result | final colour + opacity | closure tree |
| Light loops | inside the shader | owned by the integrator |
| Secondary rays | shader may call `trace()` | chosen by the integrator |
| Path-tracing fit | needs D6 fallback | natural |

### 2.3 Closure names and signatures — FIX (use OSL's, D14)

Chat 2 used names that are **not** OSL standard closures. Use the
`stdosl.h` forms:

| Chat 2 wrote | OSL standard (`stdosl.h`) |
|---|---|
| `diffuse(N, baseColor)` | `baseColor * diffuse(N)` (weight by multiplication) |
| `microfacet_ggx(N, roughness)` | `microfacet("ggx", N, alpha, eta, refract)` or `dielectric_bsdf(N, U, refl_tint, trans_tint, rx, ry, ior, "ggx")` |
| `microfacet_ggx(N, roughness, F0)` | `generalized_schlick_bsdf(N, U, refl_tint, trans_tint, rx, ry, f0, f90, exponent, "ggx")` |
| `emission(glowColor)` | `uniform_edf(glowColor)` (`emission()` takes no arguments and is deprecated) |
| — | `transparent()` / `transparent_bsdf()`, `holdout()`, `layer(top, base)`, `oren_nayar_diffuse_bsdf`, `burley_diffuse_bsdf`, `conductor_bsdf`, `sheen_bsdf`, `subsurface_bssrdf`, `anisotropic_vdf`, `medium_vdf` |

The first registry (035) needs only: `diffuse`, `oren_nayar`, `microfacet`
(`ggx`, `beckmann`), `reflection`, `refraction`, `transparent`, `holdout`,
`uniform_edf` (and `emission` as a deprecated alias). Chat 1 §"where things
become interesting" proposed nearly the same starter set.

### 2.4 Closure storage — ADOPT (D17), layout per `oslclosure.h`

```cpp
// Mirrors OSL::ClosureColor / ClosureComponent / ClosureMul / ClosureAdd.
struct ClosureNode { int id; };                 // id >= 0: component; MUL = -1; ADD = -2
struct alignas(16) ClosureComponent : ClosureNode {
    Spectrum3 w;                                 // weight (3 channels: OSL zero-copy, D18)
    // parameter block follows the struct: data() == this + 1
};
struct ClosureMul : ClosureNode { Spectrum3 weight; const ClosureNode* closure; };
struct ClosureAdd : ClosureNode { const ClosureNode* a; const ClosureNode* b; };
```

- Arena-allocated per shading batch; POD; no virtual functions, no heap C++
  objects (chat 2b §13: GPU-friendly, compact type-tagged records).
- **Flattened fast path** (chat 2a §10.2): an additive tree lowers to a lobe
  list; non-additive trees (`layer`, nonlinear mixes) go through the generic
  evaluator.

```cpp
struct ClosureLobe {          // flattened form
    ClosureTypeID type;
    Spectrum      weight;
    ParamOffset   params;
};
// Lobe 0: diffuse  weight=(0.6,0.2,0.1)
// Lobe 1: ggx      weight=(0.04,0.04,0.04) roughness=0.3
```

- **Memory strategies** (chat 2a §10.3) — RESERVE as a documented choice:

| Strategy | Memory | Compute | Best use |
|---|---|---|---|
| immediate shade + light | low | possible repeated work | forward |
| parameter buffering | medium | efficient light reuse | per-cell material reuse (D20, 054) |
| retained closure trees | potentially high | flexible reuse | offline, BDPT |

- **Construct once, evaluate many** (chat 2a §10.3) — ADOPT: the shader runs
  once per shading point; the integrator evaluates the closure for every light
  sample without re-running textures or displacement.

### 2.5 Closure registry — ADOPT

Chat 2a §2.5; the compiler never knows how to evaluate GGX:

```cpp
registry.registerClosure({
    .name       = "microfacet",
    .category   = ClosureCategory::BSDF,      // BSDF | EDF | VDF | signal (holdout)
    .parameters = { Param::String("distribution"), Param::Normal("N"),
                    Param::Float("alpha"), Param::Float("eta"), Param::Int("refract") },
    .factory    = createMicrofacet,
});
```

The OSL backend registers into the **same** registry (no translation layer).
Closure categories (chat 1 §8): BSDF, EDF (emission), VDF (volume), signalling
(holdout) — the integrator must know the category of every component.

### 2.6 BSDF conventions — ADOPT (FIX of garbled text in chat 2a §4.3)

- `wo` points away from the surface toward the previous path vertex; `wi`
  points away from the surface toward the next vertex.
- `evaluate` returns **f(wo, wi) without the |cos θi| factor**.
- Solid-angle pdfs in sr⁻¹. **Delta lobes are explicit** (flag), never a
  fake density.
- **Forward / reverse pdf**: for a query with forward density p(wi | wo), the
  reverse density is p(wo | wi) under the same transport mode.
- **Transport mode** (`Radiance` from the camera, `Importance` from lights):
  non-symmetric scattering (refraction) scales differently.
- Converting to area measure needs geometry and therefore lives **outside**
  the BSDF (§7.2).

```cpp
enum class TransportMode { Radiance, Importance };

struct BSDFQuery  { Vec3 wo, wi; TransportMode mode; BxDFFlags requested; };
struct BSDFEval   { Spectrum f; float pdfForward, pdfReverse; BxDFFlags flags; };
struct BSDFSample { Vec3 wi; Spectrum f; float pdfForward, pdfReverse;
                    float eta; BxDFFlags flags; };

class IClosureService {                       // ADAPT: C table at the ABI (§9)
public:
    virtual BSDFEval   evaluate(ClosureHandle, const BSDFQuery&) = 0;
    virtual BSDFSample sample  (ClosureHandle, const BSDFQuery&, Sample2D u) = 0;
};
```

`Sample2D u` comes from spec 039's sample stream (integrator-owned
dimensions), never a generic RNG. Chat 2b §17: the raster/direct integrator
asks `IClosureService` for evaluations; the path tracer adds sampling; BDPT
adds reciprocal evaluations. **Integrators never walk closure trees
themselves.**

Earlier minimal form (chat 1 §5) for comparison — superseded by the above:

```cpp
struct ScatteringFunction {                   // REJECT as final shape: single pdf,
    virtual Color evaluate(const Vector3& wi, const Vector3& wo) const = 0; // no delta flag,
    virtual BSDFSample sample(const Vector3& wo, const RandomSample&) const = 0; // no mode
    virtual float pdf(const Vector3& wi, const Vector3& wo) const = 0;
};
```

### 2.7 Material outputs under the OSL way — ADAPT (D14, D16)

Chat 2 proposed an `output.*` schema. We keep the **fields as concepts** and
map them onto OSL's mechanisms:

| Concept (chat 2a §2.3) | Where it lives here |
|---|---|
| `surface` (BSDF) | BSDF components of `Ci` |
| `emission` (EDF) | `uniform_edf` (and other EDF) components of `Ci` |
| `presence` | weight of `transparent()` in `Ci` (D16) |
| `opacity` (compositing) | `Oi` |
| `transmission` | transmissive BSDF components (`refraction`, `dielectric_bsdf`) |
| `volume` / `mediumInterface` | `Interior`/`Exterior`/`Atmosphere` shaders emitting VDF closures |
| `aovs` | existing RSL `output` parameters + `DisplayChannel` |

### 2.8 Transparency: three phenomena — ADOPT

Chat 2a §8:

1. **Presence / cutout**: the surface exists or not (leaves, fences); a ray
   or sample passes through an absent surface.
2. **Compositing opacity**: foreground/background combination, possibly
   artistic, not transport.
3. **Physical transmission**: refraction/medium change; a BSDF, not alpha.

Legacy colour-valued, premultiplied `Oi` is preserved for `Surface` shaders.
Weighted-blended OIT may be an optional fast approximation, never the
reference.

### 2.9 Closures are declarative — ADOPT (D15)

Chat 2b §11: a material must not call arbitrary integrator operations while
constructing closures, or raster and path-traced evaluation diverge. Legacy
`trace()`/`gather()`/`illuminance()` stay available in the classic profile.

---

## 3. Shading languages

### 3.1 RSL 2.0 closure shader (layer 5) — ADOPT (OSL way, D14)

```c
class standard_material(color baseColor = 0.8; float roughness = 0.3;
                        float metallic = 0;)
{
    public void surface(output color Ci, Oi)   /* Ci carries a closure: see note */
    {
        normal Nf = faceforward(normalize(N), I);
        closure color d = baseColor * diffuse(Nf);
        closure color s = microfacet("ggx", Nf, roughness * roughness, 1.5, 0);
        Ci = (1 - metallic) * d + s;          /* scattering only (D15) */
        Oi = Os;                              /* compositing (D16) */
    }
}
```

Note: how `Ci` is typed as `closure color` inside a class-shader method (a
`Bxdf` binding implies it, or a declared `output closure color` parameter) is
settled in the layer-5 spec's clarify stage; the semantics above are fixed.

OSL equivalents (chat 1 §2, chat 1 light section; correct as written):

```c
surface simple_material(color albedo = color(0.8, 0.2, 0.1),
                        output closure color BSDF = 0)
{ BSDF = albedo * diffuse(N); }

surface simple_light(color light_color = color(1), float intensity = 10.0)
{ Ci = uniform_edf(light_color * intensity); }      /* not emission(): deprecated */
```

### 3.2 Rejected language proposals — REJECT

| Proposal | Source | Reason |
|---|---|---|
| `output.surface/emission/presence/opacity/medium` namespace | 2a §2.2, 2b §11 | D14: OSL way; fields map per §2.7 |
| `output.aov["name"]`, `output aov color x;` | 2a §11.4, 2b §11 | RSL `output` params + `DisplayChannel` already do this |
| `material` entry-point keyword | 2a §11.2 | a third dialect; RSL 2.0 class shaders cover it |
| typed `closure<bsdf>`, `closure<edf>`… | 2a §2.4, §11.1 | OSL has one closure type; the registry carries the category |
| explicit `ShadingContext ctx` parameter | 2a §11.3 | RSL 2.0 and OSL keep implicit globals. ADAPT the useful part: a builtin to query whether derivatives are available at this shading point |
| closures that spawn rays | chat 1 | integrator owns ray budgets |
| compiling OSL into RSL bytecode | chat 1 §6 | reproduces OSL semantics for no gain |

### 3.3 Optional language extensions — DEFER (note only)

Chat 2b §11: extension namespaces for spectral shading, polarization,
differentiable rendering, nonlocal/subsurface closures, specialized volume
transport, procedural light emission, renderer-specific closure families.

### 3.4 Physically plausible shading (layer 4) — NOTE

Pixar RPS 16 (from the 16.0 release notes and a fundza walkthrough, not Pixar's full docs):
materials and lights are class shaders implementing
`generateSamples(string type; output __radiancesample s[])` and
`evaluateSamples(string distribution; output __radiancesample s[])`;
`directlighting()` asks both sides for samples and combines them with MIS.
`__radiancesample` carries direction, radiance, materialResponse,
accumulatedMaterialResponse — **pdf and distance fields must be verified
against Pixar's docs at layer 4's specify**. These methods become a BSDF
implementation behind `IClosureService`; `directlighting()` routes into the
integrator interface (D15).

---

## 4. Lights

### 4.1 OSL has no light shader type — NOTE

Chat 1 §2–§3: lights are emissive surfaces (`uniform_edf`). Point, spot,
directional and environment lights are renderer objects; OSL may supply
programmable radiance but defines no light protocol.

| Classic RSL | OSL |
|---|---|
| `light` shader type | none |
| `illuminate()` / `solar()` | renderer-owned light sampling |
| `illuminance()` in surfaces | renderer-owned integration |
| `L`, `Cl` | no standard loop |
| `Ci` = final colour | `Ci` = closure |

### 4.2 Renderer-owned light objects — ADAPT (D10)

Chat 1 §5–§7:

```cpp
enum class LightType { Point, Spot, Directional, Rectangle, Mesh, Environment };

struct Light {                        // ADAPT: becomes the light plug-in interface
    LightType  type;
    Transform  transform;
    ShaderRef  emission;              // RSL 2.0 plausible light, OSL EDF, or null
    LightSample sample(const ShadingPoint& P);
    Spectrum   evaluateEmission(const LightSample& s);
    float      visibility(const ShadingPoint& P, const LightSample& s);
};
```

Classic `LightSource` shaders become the third kind through an adapter that
turns `illuminate`/`solar` into a **delta** sample feeding `(L, Cl)` into
`closure.evaluate()`; they cannot provide the pdfs MIS needs.

A programmable spot falloff (chat 1 §6) — ADAPT as an RSL 2.0 / OSL light
plug-in parameter shader, not a protocol:

```c
shader programmable_spot(vector light_direction = vector(0,0,-1),
                         vector direction_to_target = vector(0,0,-1),
                         float cone_angle = 0.5, float softness = 0.1,
                         color light_color = color(1), float intensity = 10,
                         output color radiance = 0)
{
    float c = dot(normalize(light_direction), normalize(direction_to_target));
    float outer = cos(cone_angle);
    float inner = cos(max(0.0, cone_angle - softness));
    radiance = light_color * intensity * smoothstep(outer, inner, c);
}
```

### 4.3 Light service — ADOPT (chat 2a §5, 2a §4.3)

```cpp
struct DirectLightSample {
    Vec3 wi; Spectrum Li; float distance;
    float pdfDirection;            // sr^-1 (area lights converted at the reference point)
    float pdfLightSelection;
    LightHandle light;
    VisibilitySegment visibility;
    bool delta;
};
struct EmissionSample {            // light subpaths: BDPT, photons
    Ray ray; Spectrum Le;
    float pdfPosition;             // area measure
    float pdfDirection;            // sr^-1
    float pdfLightSelection;
    LightHandle light;
};

class ILightService {
public:
    virtual DirectLightSample sampleDirect(const SurfacePoint& ref, Sample3D u) = 0;
    virtual float             pdfDirect   (const SurfacePoint& ref, const DirectLightSample&) = 0;
    virtual EmissionSample    sampleEmission(Sample4D u) = 0;
    virtual EmissionPdfs      pdfEmission (LightHandle, const Point3& p, const Vec3& dir) = 0;
    virtual Spectrum          evalEmission(LightHandle, const Point3& p, const Vec3& dir) = 0;
};
```

- **Evaluating** emission and its pdfs at a *given* position and direction
  (not only sampling) is required for BDPT connections and MIS — RESERVE in
  the first version.
- Measures (area vs solid angle, whether selection probability is folded in,
  delta/environment lights) must be documented.
- Samples come from 039's stream (integrator-owned dimensions).
- **Open for 041's clarify**: when a classic adapter light computes its own
  shadow (shadow map inside the light shader), how is that reported against
  `VisibilityQuality::Exact` (§5.6)?

---

## 5. Integrator API

### 5.1 Lifecycle — ADOPT (merged from 2a §3.2 and 2b §3.1)

```cpp
class IIntegrator {
public:
    virtual ~IIntegrator() = default;
    virtual IntegratorInfo info() const = 0;
    virtual Status configure(const RenderSettings&) = 0;
    virtual Status prepare(IRenderServices&, const SceneView&) = 0;
    virtual Status render (IRenderServices&, const RenderWork&, RenderContext&) = 0;
    virtual void   finish (IRenderServices&) = 0;
};
```

Conceptual C++; the frozen boundary is the C table of §9.

### 5.2 Work units — ADAPT

Chat 2a §3.2 / 2b §3.1 defined `RenderWork` as a tagged descriptor (tile,
ray batch, sample range, custom). Adaptation: our **hiders** schedule work
(buckets, rays) and hand the integrator **batches of shading points**; a
`RenderWork` is "a batch of `SurfacePoint`s plus its sample-stream identity".
Sample ranges / light subpaths become relevant with progressive or BDPT
integrators — RESERVE the tag.

```cpp
struct RenderWork {               // variant, not a raw union (chat 2b note)
    WorkDomain domain;            // ShadingBatch | SampleRange | LightSubpaths | Custom
    PixelRegion region;
    SampleRange samples;
    FrameTime   time;
    uint64_t    workId;
};
```

### 5.3 Capability negotiation — ADOPT (D21)

```cpp
enum class Capability : uint64_t {
    RayIntersection = 1ull << 0,  RayOcclusion     = 1ull << 1,
    Rasterization   = 1ull << 2,  LightSampling    = 1ull << 3,
    LightEmissionSampling = 1ull << 4, SurfaceClosures = 1ull << 5,
    VolumeClosures  = 1ull << 6,  ReversePDF       = 1ull << 7,
    Derivatives     = 1ull << 9,  PathConnections  = 1ull << 10,
    BatchedExecution= 1ull << 11, SpectralTransport= 1ull << 12,
    DeepOutput      = 1ull << 13,
};
struct IntegratorInfo {
    const char* name;
    CapabilitySet required, optional;   // integrator side
};
// hiders declare `provided`; the host rejects a hider × integrator pair with
// an explicit diagnostic when `required ⊄ provided` (D28).
```

Capabilities carry **versions and semantic guarantees**, not only bits
("ray queries" does not imply media, motion blur or spectral). Avoid an
inheritance hierarchy (`RasterIntegrator : Integrator`,
`BidirectionalIntegrator : PathIntegrator`) — chat 2a §3.3, REJECT.

### 5.4 Services — ADAPT

Chat 2a §4 / 2b §9 service set, with what each maps to here:

| Service | Responsibility | Here |
|---|---|---|
| `ICameraService` | camera sampling, projection; BDPT importance | 028 `ICamera`; importance RESERVE |
| `IGeometryService` | intersections, surface reconstruction | 029 `ITracer` + Geometry |
| `IRasterService` | binning, rasterization | **not exposed**: hiders own it (§1.1) |
| `IShadingService` | execute shaders, build closures | 033 `ShadingSystem` |
| `IClosureService` | evaluate/sample closures | 035 |
| `ILightService` | direct + emission sampling | 041 |
| `IVisibilityService` | occlusion, transmittance | 029 `ITracer`, §5.6 |
| `ISamplingService` | sample sequences | 039 |
| `IFilmService` | sample submission, AOVs | existing framebuffer/AOV path |
| `IExecutionService` | parallelism, memory | existing threads; not standardized |

```cpp
auto* shading = services.get<IShadingService>();
auto* custom  = services.queryInterface("org.openrender.guiding", Version{1, 0});
```

```cpp
class IGeometryService {
public:
    virtual Hit  intersect(const Ray&) const = 0;
    virtual void intersectBatch(Span<const Ray>, Span<Hit>) const = 0;
    virtual SurfaceGeometry evaluateSurface(const Hit&) const = 0; // no material eval
};
class IShadingService {
public:
    virtual MaterialHandle shade(const ShadingPoint&, ShadingArena&) = 0;
    virtual void shadeBatch(Span<const ShadingPoint>, Span<MaterialHandle>, ShadingArena&) = 0;
    virtual MaterialView material(MaterialHandle) const = 0;
};
```

`MaterialHandle` is valid only for its `ShadingArena`'s documented lifetime;
BDPT needs a deliberate persistent strategy (retain compact closure data,
reconstruct, or a persistent arena) — RESERVE.

### 5.5 Shading points — ADAPT (feeds 026's `ShaderGlobals`)

```cpp
struct SurfacePoint {                 // compact core (chat 2a §4.1)
    Point3 P; Normal3 Ng, Ns; Vec3 wo;
    GeometryHandle geometry; MaterialHandle material;
    AttributeContext attributes;      // uv, derivatives, primvars, ids: on demand
};
```

Not every producer can fill every field: raster fragments have screen-space
derivatives, ray hits have ray differentials or cones, BDPT light vertices may
have none. 026's `ShaderGlobals` is batch/grid-shaped and carries both grid
`du/dv` differences and `dPdx/dPdy`-style differentials, path depth, transport
mode, ray type and the shading-cell footprint (D20).

### 5.6 Exact vs approximate visibility — ADOPT

Chat 2b §9:

```cpp
enum class VisibilityQuality { Exact, Approximate };
struct VisibilityQuery  { Point3 from, to; float time; VisibilityQuality quality; };
struct VisibilityResult { Spectrum transmittance; bool valid; };
```

An unbiased integrator must never silently receive shadow-map visibility when
it asked for exact transport.

### 5.7 Transport services facade — ADOPT (chat 2b §9)

```cpp
struct ITransportServices {
    virtual ISceneService&      scene()      = 0;
    virtual IGeometryService&   geometry()   = 0;
    virtual IShadingService&    shading()    = 0;
    virtual IClosureService&    closures()   = 0;
    virtual ILightService&      lights()     = 0;
    virtual IVisibilityService& visibility() = 0;
    virtual ICameraService&     camera()     = 0;
    virtual IFilmService&       film()       = 0;
};
```

The renderer exposes transport primitives; the integrator owns the algorithm.

---

## 6. The direct integrator (the "raster" integrator)

### 6.1 Pipeline — ADAPT (chat 2a §7.1, 2b §12)

```text
1 geometry prep      tessellation · displacement · motion      (hider, Geometry)
2 projection          clipping · bounds · bucket binning        (reyes/zbuffer hider)
3 visibility          coverage · depth · ids                    (hider)
4 reconstruction      P · N · uv · derivatives                  (hider → ShaderGlobals)
5 shading             execute shaders → closures                (ShadingSystem, batched)
6 closure integration direct lights · shadows · IBL             (Integrator "direct")
7 film                transparency · filtering · AOVs           (hider)
```

Displacement happens **before visibility** (chat 2b §12.3): it modifies
geometry and is never a surface closure. REYES dicing keeps RSL's
procedural-displacement strength.

### 6.2 Estimators — FIX (chat 2a §7.3 lost its symbols)

Deterministic (delta) lights:

$$L_o(\omega_o) \approx L_e(\omega_o) + \sum_{j=1}^{M} f(\omega_o,\omega_{i,j})\,L_{i,j}\,V_j\,\max(0,\mathbf n\cdot\omega_{i,j})\,w_j$$

where $w_j$ carries the light-sampling estimator's normalization (1 for a
point light under its radiometric convention). Sampled area lights:

$$\widehat L_o = L_e + \frac{1}{M}\sum_{j=1}^{M}\frac{f(\omega_o,\omega_{i,j})\,L_i(\omega_{i,j})\,V_j\,|\mathbf n\cdot\omega_{i,j}|}{p(\omega_{i,j})}$$

Area-sampled form (chat 1 light section): $L_o \approx f_r\,L_e\,V\,G / p_A(x_l)$
with $G = |\cos\theta_i|\,|\cos\theta_l| / \|x-x_l\|^2$.

### 6.3 Pseudocode — ADAPT (chat 1 §7, chat 2a §7.4, 2b §12.1)

```cpp
// batched by material (chat 2b §12.1); the hider supplies the batch
void DirectIntegrator::render(IRenderServices& s, const RenderWork& work, RenderContext& ctx)
{
    for (MaterialBatch& batch : work.materialBatches()) {
        ShadingBatchResult m = s.shading().shadeBatch(batch.material, batch.points, ctx.arena);
        for (size_t k = 0; k < batch.points.size(); ++k) {
            const SurfacePoint& sp = batch.points[k];
            Spectrum Lo = evalEmission(m[k].emission, sp);          // EDF components
            for (int j = 0; j < lightSamples; ++j) {
                DirectLightSample ls = s.lights().sampleDirect(sp, ctx.sample3D(sp, j));
                if (!ls.delta && ls.pdfDirection == 0) continue;
                BSDFEval f = s.closures().evaluate(m[k].surface, {sp.wo, ls.wi, TransportMode::Radiance});
                VisibilityResult v = s.visibility().evaluate({sp.P, ls.endpoint(), sp.time,
                                                              VisibilityQuality::Approximate});
                // delta lights carry no solid-angle density: Li is already the
                // incident radiance-equivalent, so divide only by the selection pdf
                float pdf = ls.delta ? ls.pdfLightSelection
                                     : ls.pdfDirection * ls.pdfLightSelection;
                Lo += f.f * ls.Li * v.transmittance * absDot(sp.Ns, ls.wi) / pdf / lightSamples;
            }
            s.film().accumulate(sp, Lo);                             // presence/Oi handled by hider
        }
    }
}
```

### 6.4 Lighting strategies — NOTE (chat 2a §7.3)

| Strategy | Fits direct? | Note |
|---|---|---|
| per-light direct evaluation | yes | first implementation |
| shadow maps | yes | approximate visibility (§5.6) |
| ray-traced shadows | yes | needs ray queries |
| environment importance sampling | yes | glossy materials |
| irradiance cache / probes | yes | D26: bxdf-gi spec |
| reflection probes, SSR | — | out of scope (offline renderer) |
| reservoir resampling | later | needs sampling infrastructure |
| path-traced indirect | hybrid | pathtracer integrator |

### 6.5 Validation tests — ADOPT

- **Chat 1 §7 / §8**: render the same simple diffuse material through REYES and
  raytrace shading paths with identical direct lights → equal within the hider
  parity thresholds (validates visibility/shading separation before path
  tracing).
- **Classic vs Bxdf**: classic `plastic.sl` against the built-in plastic Bxdf
  under the same lights → equal within BRDF-convention tolerance.
- **Direct-lighting equality** across hiders (chat 2b §16 phase 3).
- Chat 1 §7's checklist: identical geometric/attribute inputs; derivatives and
  texture filtering consistent; overhead of closure evaluation measured;
  grid-batched execution efficiency.

---

## 7. Path tracing and BDPT — RESERVE

### 7.1 Path vertex (chat 2a §9.1, 2b §9.1)

```cpp
enum class VertexType { Camera, Surface, Medium, Light, Environment };
struct PathVertex {                 // tagged; camera/light/medium vertices differ
    VertexType   type;
    Point3       position; Normal3 geometricNormal; Vec3 incomingDirection;
    Spectrum     throughput;
    float        pdfForward, pdfReverse;    // AREA measure at non-environment vertices
    ClosureHandle scattering; MediumHandle medium;
    TransportMode mode; uint32_t flags;     // delta, wavelength state …
};
```

Direct/raster integrators never build path vertices.

### 7.2 Solid angle → area — FIX (chat 2a §9.1 / 2b §10 garbled)

For a direction sampled at $x$ that reaches surface vertex $y$:

$$p_A(y \mid x) = p_\omega(\omega_{xy} \mid x)\,\frac{|\mathbf n_y \cdot (-\omega_{xy})|}{\|y-x\|^2}$$

Valid for ordinary finite surface vertices; environment endpoints, delta
events and medium vertices need their own rules. This conversion uses
geometry and is never done inside a BSDF.

### 7.3 Connections (chat 2b §9.2)

```cpp
struct ConnectionResult { Spectrum contribution; float geometryTerm;
                          Spectrum transmittance; bool visible; };
ConnectionResult connect(const PathVertex& a, const PathVertex& b, const ConnectionOptions&);
```

MIS weighting stays **inside the integrator** (or an optional path-space
library); different algorithms keep different bookkeeping.

### 7.4 BDPT checklist (chat 2b §10) — the closure API is validated against it from day one

Camera importance evaluation + sampling (lens position and direction
densities); light emission evaluation + sampling; BSDF/phase evaluation in
both transport directions; forward and reverse pdfs with explicit measures;
delta scattering, delta lights, delta cameras; medium transitions and
transmittance; solid-angle ↔ area conversion; connection visibility and MIS.
035's tests include reciprocity and reverse-pdf consistency even though no
BDPT exists.

```cpp
class ICameraService {              // RESERVE in 028's ICamera
public:
    CameraRaySample        sampleRay(const CameraSample&);
    CameraImportance       evaluateImportance(const Ray&);
    CameraConnectionSample sampleConnection(const Point3& ref, const CameraSample&);
};
```

Future integrators sharing these primitives (chat 2a §9.4): VCM, photon
mapping, (progressive) photon mapping, Metropolis, light tracing.

---

## 8. Shading frequency, derivatives, visibility buffers

### 8.1 Shading rate — ADOPT (D20)

Chat 1 §5 proposed an explicit shading-frequency policy (`grid` vs `sample`).
Superseded by the maintainer's decision: `ShadingRate` itself is the control,
with the same meaning in every hider. Measured on master (2026-10-15):

| | REYES | raytrace (before 054) |
|---|---|---|
| shading evaluations | set by `ShadingRate` via dicing (`object.cpp:339`; stored as √area, `rendererContext.cpp:2319`) | one per camera sample; `ShadingRate` ignored |
| filter footprint | `ShadingRate` × pixel (`shading.cpp:910–913`) | 1 pixel (`raytracer.cpp:715`) |
| geometric resolution | `ShadingRate` | ray footprint (`HIDER_PARITY.md`) |

`RiShadingInterpolation` is unimplemented (always smooth,
`rendererContext.cpp:2323`), so samples inside a micropolygon get
interpolated values, not one flat value.

054's design: camera hits are grouped per object per bucket; the hit region
is diced with `estimateDicing`, its vertices shaded through the grid path,
and each hit interpolates at its (u,v). Footprint √ShadingRate pixels. Cells
larger than a pixel (ShadingRate > 1) need no cross-pixel bookkeeping because
the grid lives on the object. Raytrace intersection tessellation is unchanged
(displacement detail under raytrace stays a separate, later question).
To verify at plan: bucket-independent dicing, cost for sparsely hit objects.
Deferred REYES per-sample shading (§8.2) becomes a performance option only.

### 8.2 Visibility buffer — ADAPT (chat 2a §7.2)

```cpp
struct VisibilitySample {
    uint32_t primitiveID, instanceID;
    Vec2     barycentric;   // for micropolygons: grid (u,v) + quad id
    float    depth;
    uint32_t coverageMask;
};
```

Reconstruct attributes only for visible samples; procedural materials stay
arbitrary; no fixed G-buffer. Pixel samples ≠ fragment records under MSAA;
transparent/multi-layer visibility needs extra handling. Basis of the
deferred REYES per-sample spec.

### 8.3 Derivatives — ADOPT (chat 2b §12.2, chat 1 JIT §5)

Perspective-correct interpolation; discontinuities at primitive boundaries;
displacement changes the footprint; texture filtering needs consistent
footprints; procedurals need derivatives of arbitrary expressions. Strategies
(dual numbers, generated derivative code, grid finite differences) are not
interchangeable at discontinuities and divergent control flow. Derivatives are
optional-but-standardized in the shading context, requested lazily.

---

## 9. Plug-in ABI and versioning — ADOPT (D7, D22)

Chat 2b §14:

```c
/* C ABI: versioned function tables, explicit sizes, opaque handles,
   defined ownership, error codes. */
typedef struct IntegratorPluginDescriptor {
    uint32_t     structSize;          /* host checks >= known size */
    uint32_t     abiVersion;          /* MAJOR<<16 | MINOR<<8 | PATCH (037 convention) */
    const char*  name;
    CapabilitySet required;
    CapabilitySet optional;
    OrIntegrator* (*create)(const OrHostAPI* host, const OrSettings*);
    void          (*destroy)(OrIntegrator*);
    OrStatus      (*render)(OrIntegrator*, const OrRenderWork*, OrRenderContext*);
    /* … */
} IntegratorPluginDescriptor;

extern "C" bool registerIntegratorPlugin(const OrHostAPI* host,
                                         IntegratorPluginDescriptor* out);
```

- Same shape for Bxdf, light, light filter, display filter, sample filter,
  pattern and hider plug-ins. A C++ convenience header wraps each table.
- Host rejects a MAJOR mismatch with a clear diagnostic (037).
- Extensions through namespaced `queryInterface("org.example.renderer.guiding", 1)`.
- Built-in implementations register through the same tables (D8).

What we **standardize** vs **not** (chat 2b §15) — ADOPT as scope guidance:

| Standardize | Leave out |
|---|---|
| surface-interaction semantics | BVH implementation |
| closure evaluation/sampling conventions | rasterizer edge-walking/binning |
| light and camera sampling contracts | thread scheduler |
| pdf measures, transport modes | GPU command buffers |
| visibility/transmittance query semantics | path guiding, reservoir algorithms |
| film sample submission, AOV naming | denoisers |
| capability negotiation, extension discovery, versioning | shader compiler backend, film storage layout |

---

## 10. JIT evolution (chat 1) — DEFER to the native-JIT-lowering spec (D27)

Context: today's `.slo` JIT is "Level 1": generated code calls the
interpreter's `op_*`/`rsl_*` functions. Bytecode operates on named variables
and typed slots, which suits LLVM lowering.

### 10.1 Three levels

```cpp
// Level 1 — JIT calls interpreter operations (today)
op_add(vm, dst, a, b);  op_mul(vm, dst, c, dst);  op_texture(vm, dst, tex, uv);

// Level 2 — native arithmetic, runtime service calls (target first)
float c = a + b;  float d = c * roughness;  Color tex = renderer_texture(/*…*/);

// Level 3 — specialized SIMD grid kernels (keep possible)
shade_grid_8(const ShadingInputs8* in, MaterialOutputs8* out);
```

```text
MUL_FLOAT temp1, roughness, 0.5        %a  = fmul float %roughness, 5.0e-01
ADD_FLOAT temp2, temp1, 0.1     ──►    %b  = fadd float %a, 1.0e-01
MUL_COLOR Ci, Cs, temp2                %r  = fmul float %Cs_r, %b   (…g, …b)
```

### 10.2 Instruction categories

| Category | Lowering |
|---|---|
| arithmetic, comparisons, basic vector ops (`dot`, `cross`, `normalize`, `mix`) | native LLVM instructions |
| control flow, assignments, locals | native control flow + SSA |
| texture, trace, attributes, illumination loops | calls through a stable runtime ABI |

### 10.3 Lowering table with fallback

```cpp
using LoweringFunction = void (*)(LLVMCodegen&, const Instruction&);
std::unordered_map<Opcode, LoweringFunction> lowering;
lowering[OP_ADD_FLOAT] = lowerAddFloat;
lowering[OP_MUL_FLOAT] = lowerMulFloat;
lowering[OP_TEXTURE]   = lowerTextureCall;   // runtime call

void lowerMulFloat(LLVMCodegen& cg, const Instruction& inst) {
    auto* lhs = cg.load(inst.source1);
    auto* rhs = cg.load(inst.source2);
    cg.store(inst.destination, cg.builder.CreateFMul(lhs, rhs));
}
```

The fallback must not assume values still live in VM slots: materialize
native values into slots before a fallback call and reload afterwards.

### 10.4 Slots: logical vs physical

```cpp
enum class Variability { Uniform, Varying };
struct SlotInfo { Type type; Variability variability; StorageClass storage; bool addressTaken; };
// storage classes: local temp, shader parameter, renderer global, output,
// uniform, varying, address-taken
llvm::DenseMap<SlotID, llvm::Value*> currentValues;  // within a block
```

Start with promotable `alloca`s + `mem2reg` across branches/loops (simpler than
hand-built PHIs). Opaque offsets into a VM buffer defeat promotion.

### 10.5 Uniform/varying and masks

```cpp
float temp1 = roughness * 0.5f;                       // uniform: once per grid
for (int lane = 0; lane < gridSize; ++lane) temp2[lane] = Cs[lane] * temp1;
```

Uniformity is derived conservatively (varying globals, divergent branches and
renderer callbacks make results varying). Divergent `if` needs execution
masks / predication. **Prerequisite: GitHub #8** (the uniform fast path drops
`if` conditions) — native lowering builds on exactly that logic.

### 10.6 Bitcode libraries and measurement

Selective inlining of math (`fresnelSchlick`, `ggxDistribution`,
`smithMasking`) from bitcode; watch compile time and code size. **Benchmark
first**: five representative shaders × {VM, current JIT, lowered JIT};
measure JIT time, code size, runtime calls per point, allocations, grid-size
scaling, correctness vs the VM (derivatives, divergence). The VM stays the
reference backend.

Development order (chat 1 §8): scalar arithmetic + control flow → typed SSA
cleanup → uniform/varying → SIMD + masks → derivatives/procedurals → (OSL is
independent) → shared optimizations only after profiling.

---

## 11. OSL integration notes

### 11.1 Adapter steps — ADOPT (chat 1 §6)

1. Implement `RendererServices` over our texture, transform, attribute,
   userdata and ray-query systems (026's v2 contract is shaped for this).
2. Create an OSL `ShadingSystem`; build, connect and cache shader groups
   (034's networks).
3. Translate shading points into `ShaderGlobals` (REYES grids and ray hits).
4. Register supported closures into **our** registry (D17); evaluation is
   ours, independent of OSL's execution engine.
5. Connect integrators.

OSL closure values cannot be inspected by shader code; their meaning is
entirely the renderer's. References: `testshade` (execution without a
renderer), `testrender` (renderer integration incl. closures), "OSL in Ray
Tracing in One Weekend".

### 11.2 Two JITs, one LLVM — ADOPT

Share LLVM as a dependency, not a JIT runtime: OSL manages its own group
specialization, runtime functions, memory layouts, closure allocation and
thread-local contexts. OSL batched execution (SIMD 4/8/16) is benchmarked
against our grid JIT before any sharing.

### 11.3 Dependency facts (verified)

- OSL `INSTALL.md`: LLVM **14–23** with Clang libraries; OIIO **≥ 3.0**;
  Imath ≥ 3.1; pugixml ≥ 1.8; flex/bison; `cmake_minimum_required(3.23)`.
  Boost not listed. Partio, Qt, Python, OptiX optional (left out).
- Ubuntu 24.04: LLVM 18 default (14–20 available); OIIO 2.4.17; no OSL
  package.
- vcpkg (baseline `319504a5`): LLVM 23.1.1 (history jumps 18.1.6 → 23.1.1);
  OIIO 3.1.14 with OpenColorIO mandatory; no OSL port (overlay port needed).
- **OIIO footprint, macOS arm64, static triplet** (measured 2026-10-15): 23
  ports; 3.8 min build; 57 MB installed (`libOpenImageIO.a` 21.7 MB,
  `libOpenColorIO.a` 11.8 MB); a minimal stripped read/write binary is
  10.6 MB with OIIO vs 2.3 MB with libtiff + libpng + OpenEXR → **≈ +8.3 MB
  per binary linking OIIO statically**, hence D23's "exactly one shared
  library". Not yet measured: LLVM `clang` feature, OSL itself, Linux.

---

## 12. GPU and roadmap notes — DEFER

Chat 2b §13 GPU stages (CPU raster + CPU shading → SIMD shading → GPU raster →
GPU shading → GPU ray queries → wavefront) are out of scope. The one rule
carried over: closure records stay compact, type-tagged and arena-allocated
(§2.4).

Chat 2b §16 roadmap (closures first, path tracer before raster) — REJECT as
ordering: the maintainer's order is direct integrator with C++ Bxdfs → RSL 2.0
layers → OSL → path tracer.

Chat 2b's three open decisions are settled: compatibility-preserving RSL
frontend (D11–D12); compact closure IR with a flattened fast path (D17);
offline REYES-style raster backend, not GPU.

---

## 13. Spec → section index

New-spec numbers (041–055) are provisional, confirmed at `specify`; the plan file holds the briefs.

| Spec | Read |
|---|---|
| 025 graphics-state-handles | D5, D9 |
| 026 shading-contract-v2 | §5.5, §8.1, §8.3, §11.1 |
| 028 camera-and-motion-layer | §7.4 (`ICameraService` reserve) |
| 029 hider-shading-separation | §5.4, §5.6 |
| 030 hider/integrator registry | §1, §5.1–§5.3, §9, D2, D4, D8, D21, D28, D30 |
| 033 shading-system-backends | §5.4 (`IShadingService`), D25 |
| 034 shader-networks | D9, §11.1 |
| 035 material interface + closure model | §2, §9, D13–D19 |
| 036 RSL 2.0 layer 5 (closures) | §2.3, §2.7, §2.9, §3.1–§3.2, D14–D16 |
| 037 plugin-abi-semver | §9, D7, D22 |
| 039 sampling-foundation | §2.6 (`Sample2D`), §4.3, §8.1 |
| 040 gi-determinism | — (unchanged) |
| 041 light-service | §4, D10, D30 |
| 042 direct-integrator (milestone) | §1.2, §5, §6, D6, D28 |
| 043 shader-compiler-library | D12, D25 |
| 044–047 RSL 2.0 layers 1–4 | D11–D12, §3.4 (layer 4), D15 |
| 048 oiio-image-io | D23, D30, §11.3 |
| 049 osl-shading-backend | §11, D17 (layout test), D23–D24 |
| 050 pathtracer-integrator | §7, D4, D6 |
| 051 ris-plugin-types | §9, D7–D8, D30 |
| 052 shading-runtime-library | D25 |
| 053 bxdf-gi | D26, §6.4 |
| 054 shading-rate-unification | §8.1–§8.3, D20 |
| 055 native-jit-lowering | §10, D27 |
