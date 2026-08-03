# Status note (2026-08)

This audit is a point-in-time snapshot from before spec `008-hider-parity-convergence`
executed the plan above; its analysis and reasoning remain accurate for the state they
describe, but the divergence table (Phase 2) and execution order (Phase 5) are no longer
current. Current status of every D1-D10 item lives in
`DEVNOTES_DETAILS/HIDER_PARITY.md`. Summary:

- **Closed** (via R1-R4/S1-S5, Option A): D1 (lens sample distribution — fixed pre-branch
  in spec 007-dof-disk-sampling), D2 (pixel-jitter-constant drift — unified via the new
  shared `CSampler`), D5 (displacement default parity), D6 (transparent-hit AOV
  compositing), D7 (depth-filter modes / `zvisibilityThreshold` in raytrace), D8 (matte
  semantics), D10 (raytraced object motion blur — verified working, not actually broken).
- **Permanent, documented residuals** (per this audit's own Option A/B/C framing, not
  closable by refactor): D3/D4 (shading-density/interpolation model — grid-interpolated
  vs. per-hit shading) and D9 (DOF occlusion model — screen-space scatter vs. true lens
  rays).
- Option B (unified per-bucket sample table) also landed, gated behind the internal-only
  `OPENRENDER_CORRELATED_SAMPLE_TABLE` diagnostic env var.
