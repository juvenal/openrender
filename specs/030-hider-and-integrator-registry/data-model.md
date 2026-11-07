# Data model: Hider and Integrator Registry (spec 030)

Entities from spec.md (Key Entities), with their fields, validation rules and lifecycles. Interfaces are in
[contracts/](contracts/); decisions in [research.md](research.md).

## Capability (Transport)

| Field | Type | Rule |
|---|---|---|
| name | `const char *` | one of the vocabulary names (contracts/integrator-interface.md); exact, case-sensitive |
| version | `uint32_t` | `OR_ABI_VERSION` encoding; all 1.0.0 in this spec |

A hider *provides* a set; an integrator *requires* a set and *optionally uses* a set. Requirement met:
same name and `provided >= required`.

## Hider kind (Hiders) = `OrHiderTable`

| Field | Rule |
|---|---|
| name | non-empty, unique across names and aliases of all hider kinds |
| aliases[] | each `{name, deprecationWarning?, integrator?}`; name unique; `integrator` must be `NULL` (warned and ignored otherwise, FR-014) |
| provides[] | capabilities; may be empty |
| dispatch | `BUCKETS` or `PHOTONS` |
| preDisplaySetup | optional |
| create / destroy | required; one instance per render thread per frame |
| header | `structSize >= sizeof(OrHiderTable)`; `abiVersion` MAJOR 2 |

Built-ins: `raytrace`, `reyes` (aliases `hidden`, `stochastic`-deprecated), `zbuffer`, `photon`
(contracts/hider-registration.md).

## Integrator kind (Transport) = `OrIntegratorTable`

| Field | Rule |
|---|---|
| name, aliases[] | as for hider kinds, in their own namespace (a hider and an integrator may share a name) |
| requires[], optional[] | capabilities |
| create / destroy | one instance per rendered frame |
| configure / prepare / finish | required; called once per rendered frame, in that order |

Built-in: `classic` (requires none, optional none).

## Registry (one per kind family)

Process-wide, filled once on first access (`std::call_once`): built-ins in fixed order, then static
registrars in append order. Immutable afterwards. Lookup by name or alias. Enumerable in registration order.

```text
[empty] --first access--> [built-ins added] --> [static registrars added] --> [frozen]
                         (each add: validate -> version check -> store | refuse with one diagnostic)
```

## Alias resolution (statement time, `RiHiderV`)

| Input | `COptions::hider` | Diagnostic |
|---|---|---|
| a registered name | the name | none |
| a plain alias (`hidden`) | the target's name (`reyes`) | none |
| a deprecated alias (`stochastic`) | the target's name | the alias's warning, once per statement |
| an unknown name | the name as written | none here; at frame begin (`Hider "<n>" unavailable`) |

## Integrator choice (Graphics State, `COptions`)

| Field | Default | Rule |
|---|---|---|
| integrator | `"classic"` | replaced by each `Integrator` statement, last wins |
| integratorHandle | `NULL` | the reserved string `handle` parameter |
| integratorParameters[] | empty | `{name, TYPE_*, numItems, data}`, deep-copied with the options; replaced with the name |

## Per-frame transport slot (renderer core, `SFrameTransport`)

Fields in contracts/frame-transport.md. Lifecycle per frame:

```text
           beginFrame                                   endFrame
[idle] --resolve+check--> [refused] ------------------------------------------> [idle]
          |                                                       (no finish, no hiders)
          +--create+configure--> [configured] --hiders built, prepare OK--> [prepared]
                                                                    --finish+destroy--> [idle]
```

Read-only for render threads between `prepare` and `finish`. Cleared after `endDisplays()`.

## Peer group (Hiders, `CHiderPeers`)

`{ CHider *const *members; int count; }`, one per frame, owned by the slot; `members` is the core's
`contexts` array, `count` is `numThreads`. Handed to every per-thread reyes-family hider at construction;
used for the sibling insertion loop (`reyes.cpp:1992-1993`).

## Pair check result (`SPairVerdict`)

`accepted`; `shortfalls[]` each `{name, required, provided, missing}`; `absentOptional[]`. Refusal when
`shortfalls` is non-empty. Produced once per frame, on the resolved pair.

## Removed entities

- `CSphereLight`, `CQuadLight` (`src/ri/hcshader.{h,cpp}`), the `getShader` substitute.
- `HIDER_RGBAZ_ONLY`, `HIDER_NODISPLAY`, `HIDER_ILLUMINATIONHOOK`, `HIDER_PHOTONMAP_OVERWRITE`,
  `HIDER_GEOMETRYHOOK` (`renderer.h:46-52`); `HIDER_BREAK` (2) and `HIDER_DONE` (8) stay.
- The static `preDisplaySetup()` of `CRaytracer`, `CStochastic`, `CZbuffer`, `CPhotonHider`.
