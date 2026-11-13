# Contract: Transport domain, architecture check and records

**Research**: R11, R12, R15, R16. **Requirements**: FR-025, FR-029 to FR-034.

## Constitution amendment (written at implement, with the maintainer's approval)

Through the `speckit-constitution` workflow, as a MINOR step **1.3.0 → 1.4.0** (a domain added; no
principle removed or redefined). The plan lists the change; it does not make it. It remains an
implement-time approval point (maintainer, 2026-11-07).

- Domain table, new row: `Transport | Integrators (transport strategies) and the capability vocabulary hiders and integrators negotiate with | src/ri/transport/`.
- Allowed-dependency table: new row `Transport | Graphics State, Geometry, Shading`; the Hiders row becomes
  `Graphics State, Geometry, Shading, Display Outputs, Transport`. No other row changes (spec 032 treats
  the table as fixed; 030 changes no row 032 relies on).
- Text: "nine domains" wording, if any, becomes ten. Transport depends on no Hiders, Scene Interface or
  renderer-core file (rule 2's acyclicity covers it).
- Sync Impact Report: version line, modified principle VIII (domain list and table), templates reviewed
  (`plan-template.md`'s Principle VIII checklist names the domains and gains Transport), follow-up: none.
- D30 (3) (rule 4 extended to every plug-in type) is **not** part of this amendment: it lands with spec 051.

## `tests/architecture/`

| File | Change |
|---|---|
| `domain-rules.txt` | header and "Source" line to the amended version (v1.4.0); `Transport -> GraphicsState Geometry Shading`; `Hiders -> GraphicsState Geometry Shading Display Transport` |
| `domain-map.txt` | `prefix src/ri/transport/ Transport` (new section); delete `prefix src/ri/hcshader. Core` |
| `archcheck/config.h`, `config.cpp`, `main.cpp` | `Class::Transport` (after `Display`, before `Core`), `kClassCount = 13`, `kClassNames`, help text; Transport is layered (takes part in the acyclicity check) |
| `allowlist.txt` | delete 3 DB-011 lines: `photon.cpp -> _ZN9CRenderer10hiderFlagsE`, `reyes.cpp -> _ZN9CRenderer8contextsE`, `zbuffer.cpp -> _ZN9CRenderer10hiderFlagsE`; correct the DB-011 section header count (164 → 161); add nothing |
| `metrics-baseline.txt` | Hiders row lowered to the measured values (expected -3 references, -2 members); a `Transport 0 0` row if the report lists every domain class; TOTAL follows |
| `ribvector-core-residual.txt` | unchanged |
| `fixtures/transport-allowed/` | new: a Transport file including a Shading header and a Hiders file including a Transport header; expected: pass |
| `fixtures/transport-cycle/` | new: a Transport file including a Hiders header; expected: `VIOLATION` (Transport → Hiders) and a `CYCLE` line |

The amendment and the first three rows land in the same phase as the first file under
`src/ri/transport/` (ordering gate, research R11).

## `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`

- **DB-013** moves to Resolved: "Resolved by spec 030, <date>": the hider and integrator registries, the
  capability declarations, `hcshader` deleted; measured end state (no concrete hider header, class name or
  name comparison in `renderer.cpp`, `rendererContext.cpp`, `rendererStatics.cpp`, `rendererDisplay.cpp`,
  `rendererJobs.cpp`); the evidence line numbers corrected (`renderer.cpp:778-792` and `:815-834`,
  `rendererContext.cpp:1554`, `hcshader` 798 lines); the spec 025 history note about `lightInstanceOf()`
  closed (the cast is always valid now).
- **DB-011**: a history entry for spec 030 (3 lines deleted, 164 → 161; the Hiders metric as measured);
  Size updated; Tracking keeps 031 (the remaining hider statics).
- **DB-020**: unchanged (spec R2; its stale "030" tracking text is rewritten by spec 032's FR-022).
- Architecture-check section: the class list gains Transport where classes are listed.

## Other records (end of implementation, FR-034)

- `CLAUDE.md`: Hider-system paragraph (the registration recipe replaces "new `#include` + `if/else if`
  branch"), Architecture (ten domains, Transport, the integrator registry), Testing (new labels if any),
  Spec-kit index, Active Technologies and Recent Changes entries, gotcha 35's `contexts` sentence, one new
  gotcha (registries: built-ins reached by the call graph, statement-time aliases, named vs resolved hider for
  the TSM check, the refused frame).
- `DEVNOTES.md`: status row for spec 030.
- `NEWS`: `RiIntegrator`/`Integrator`; the `spherelight`/`quadlight` behaviour change; the pair refusal.
- `doc/Documentation/Hardcoded_shaders.html`: replaced by a retirement note, not deleted (maintainer answer Q6, 2026-11-07).
- `DEVNOTES_DETAILS/PLUGIN_ABI.md`: the two new families listed as internal until spec 051, with their
  version-history tables quoted (the doc-path checks, `PluginAbi_DocPaths`, only cover installed families;
  verified at implement).
