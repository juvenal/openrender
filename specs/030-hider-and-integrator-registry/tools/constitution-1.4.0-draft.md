# Draft: constitution amendment 1.3.0 -> 1.4.0 (decision D30, spec 030)

**Status**: DRAFT for the maintainer's approval (tasks.md T037, PAUSE 3a). Nothing in `.specify/` has been edited.
After approval it is applied through the `speckit-constitution` workflow (T038) with this text as its input, declining
the git hooks.

## What changes, and why

Spec 030 adds an integrator registry and a capability vocabulary that hiders and integrators negotiate with. They need a
home that is neither the Hiders domain (an integrator is not a visibility strategy) nor the renderer core (no domain may
depend on it, and Hiders must be able to include the vocabulary). The amendment adds a **tenth domain, Transport**
(research R11, contracts/architecture-and-records.md):

- **Domain table**: a row `Transport | Integrators (transport strategies) and the capability vocabulary hiders and
  integrators negotiate with | src/ri/transport/`.
- **Allowed-dependency table**: a row `Transport | Graphics State, Geometry, Shading`, and the Hiders row becomes
  `Graphics State, Geometry, Shading, Display Outputs, Transport`. No other row changes (spec 032 treats the table as fixed).
- **Acyclicity**: one sentence under the table that Transport depends on no Hiders, Scene Interface or renderer-core file
  (Hiders depend on it, so rule 2 forbids the reverse edge); `archcheck` enforces it (the `transport-cycle` fixture proves it).
- **Version**: MINOR (a domain added; no principle removed or redefined): 1.3.0 -> 1.4.0, Last Amended 2026-11-09.
- **Sync Impact Report**: version line, modified principle VIII, templates reviewed; the plan template's Principle VIII
  checklist names the domains and gains Transport. The constitution carries no "nine domains" wording today
  (`grep -n -i 'nine\|9 domain' .specify/memory/constitution.md` finds no such wording); the class count lives in
  `tests/architecture/archcheck/config.cpp` (T043).
- **Not in this amendment**: D30 part 3 (rule 4, the semantic-versioning duty, extended to every plug-in type) lands with
  spec 051. Rule 4 and the "Existing violations" rule are untouched.

## What follows once approved (Phase 3, tasks T038 to T045)

`tests/architecture/domain-rules.txt` (v1.4.0 header; `Transport -> GraphicsState Geometry Shading`; the Hiders row gains
Transport), `domain-map.txt` (`prefix src/ri/transport/ Transport`), the `Transport` class in `archcheck`
(`kClassCount` 12 -> 13), and the two fixtures `transport-allowed` and `transport-cycle`. Nothing under `src/ri/transport/`
exists until then.

## The planned `git diff`

Exactly these two files change (the diff of this draft against the tree):

```diff
--- a/.specify/memory/constitution.md
+++ b/.specify/memory/constitution.md
@@ -1,20 +1,21 @@
 <!--
 Sync Impact Report:
-Version change: 1.2.0 → 1.3.0 (MINOR: Principle VIII rule 4 expanded to allow a host to support
-  more than one MAJOR of a plug-in interface; the rejection duty and its diagnostic are kept)
-Modified principles: VIII. Domain Separation and Modular Boundaries, rule 4 (MAJOR check now reads
-  "not one the host supports"; multi-MAJOR support through a confined adapter permitted, each
-  supported MAJOR listed in the interface's version history, dropping one is a version change)
+Version change: 1.3.0 → 1.4.0 (MINOR: Principle VIII gains a tenth domain, Transport; no principle is
+  removed or redefined)
+Modified principles: VIII. Domain Separation and Modular Boundaries (domain table: new row
+  "Transport"; allowed-dependency table: new row "Transport" and the "Hiders" row gains Transport; a
+  sentence that Transport depends on no Hiders, Scene Interface or renderer-core file)
 Added sections: None
 Removed sections: None
 Templates requiring updates:
-  ✅ .specify/templates/plan-template.md - reviewed, Principle VIII gate names no rule 4 wording
+  ✅ .specify/templates/plan-template.md - Principle VIII gate: Transport added to the list of domains
   ✅ .specify/templates/spec-template.md - reviewed, no change needed
   ✅ .specify/templates/tasks-template.md - reviewed, no change needed
   ✅ .specify/templates/checklist-template.md - reviewed, no change needed
   ✅ .specify/templates/agent-file-template.md - reviewed, no change needed
-  ✅ specs/037-plugin-abi-semver/spec.md - its "Constitution amendment needed" note is now settled
-Follow-up TODOs: None (spec 037 implements the rule; registry DB-003, issue #26)
+  ✅ tests/architecture/domain-rules.txt - header and Transport/Hiders rows brought to this version (spec 030, T039)
+Follow-up TODOs: None (spec 030, decision D30 part 1; D30 part 3, rule 4 for every plug-in type, is NOT
+  part of this amendment and lands with spec 051)
 Pending, intentionally not included: constitution-v1.1.1-draft.md (Principle VII PATCH) remains
   unratified; if ratified later it lands on top of this version.
 -->
@@ -65,6 +66,7 @@
 | Graphics State | Attributes, options, transforms, the hierarchical state stack | `src/ri/state/` |
 | Geometry | Primitives, tessellation/dicing, bounds, intersection | `src/ri/geometry/` |
 | Hiders | Visibility determination and sampling strategy | `src/ri/hiders/` |
+| Transport | Integrators (transport strategies) and the capability vocabulary hiders and integrators negotiate with | `src/ri/transport/` |
 | Shading | Shader loading, the RSL runtime/JIT, built-ins, shading-state evaluation | `src/libshader/` |
 | Texturing | Texture, 3D-texture, brick-map and point-cloud lookup, and tile paging | `src/ri/texture/` |
 | Shader DSO Operands | External shadeop extensions callable from RSL | `shadeop.h` |
@@ -85,9 +87,13 @@
 | Scene Interface | Graphics State, Geometry |
 | Geometry | Graphics State, Geometric Procedures |
 | Shading | Graphics State, Texturing, Shader DSO Operands |
-| Hiders | Graphics State, Geometry, Shading, Display Outputs |
+| Transport | Graphics State, Geometry, Shading |
+| Hiders | Graphics State, Geometry, Shading, Display Outputs, Transport |
 | Shader DSO Operands, Geometric Procedures, Display Outputs | only their own published ABI header |
 
+Transport depends on no Hiders, Scene Interface or renderer-core file: Hiders depend on it (for the
+capability vocabulary they declare), so rule 2's acyclicity forbids the reverse edge.
+
 **Rules**
 
 1. **Explicit boundary interfaces.** Domains MUST interact only through a declared interface: an
@@ -176,4 +182,4 @@
 
 All pull requests and code reviews MUST verify compliance with these principles. Complexity MUST be justified when it conflicts with simplicity principles. Use this constitution as the primary reference for all development decisions.
 
-**Version**: 1.3.0 | **Ratified**: 2025-12-07 | **Last Amended**: 2026-10-29
+**Version**: 1.4.0 | **Ratified**: 2025-12-07 | **Last Amended**: 2026-11-09
--- a/.specify/templates/plan-template.md
+++ b/.specify/templates/plan-template.md
@@ -44,8 +44,8 @@
 
 **Principle VIII — Domain Separation and Modular Boundaries** (every plan):
 
-- [ ] Domains touched are named (Scene Interface, Graphics State, Geometry, Hiders, Shading,
-      Texturing, Shader DSO Operands, Geometric Procedures, Display Outputs; or renderer core)
+- [ ] Domains touched are named (Scene Interface, Graphics State, Geometry, Hiders, Transport,
+      Shading, Texturing, Shader DSO Operands, Geometric Procedures, Display Outputs; or renderer core)
 - [ ] No new cross-domain dependency outside the constitution's allowed-dependency table, and no
       dependency on the renderer core from inside a domain
 - [ ] Every added or altered cross-domain dependency has its interface change specified in the
```
