# Phase 2: defects in approved tests, found by the first green build (a PAUSE: nothing here is changed)

The implementation (T044 to T048) is in and **48 of the 53 `closures` tests pass on Linux** (GCC 13, `-j2`). The 5 that are red are all `Bsdf_microfacet_*`
(`Reciprocity`, `PdfNorm`, `Energy`, `SamplePdf`, `ChiSquare`). I analysed every failing line of the `-V` logs (`p2-lx-test-b.log` in the Linux tree, copied to
`build/p2-lx-b-full.txt`); each one falls in one of four categories, and **none is a failure of a numerical check on a lobe that the test can resolve**. By the
rule "a check that fails fails every time: report the observed value and sigma, and change neither the check, the tolerance nor the seed", I changed nothing and
propose the fixes below for the maintainer to approve (each is a change to an approved test).

What did work, so the red set is read in context: with the directions normalised in double (a real defect of mine, found and fixed on the way: a float's 1e-7
length error is magnified in the Fresnel term near the critical angle) **every pointwise numerical check passes**: reciprocity in both modes for every config
(no `f(a,b,m) != f(b,a,dual m)` line is left), the reverse density, and the sampled-vs-evaluated `f` and densities. PdfNorm passes for every config with
eta 0 and 1.5; Energy's `<= 1 + 3 sigma` and the sampled/uniform agreement pass for every config but eta 1.0001; ChiSquare passes for every alpha 0.3 / 0.9
config with eta 0, and for most with eta 1.5.

## D1. The non-vacuity guard of `checkReciprocity` (`bsdf_checks.h`, `CL_CHECK(nonZero > o.pairs / 8)`)

The pairs are two uniform random directions. For a lobe that transmits, only a fraction of random pairs have both directions in its support (opposite sides,
and a half vector inside the visible cone); for narrow lobes (alpha 0.05) the fraction is tiny. The check fails for 7 of the 42 microfacet configs
(`Bsdf_microfacet_Reciprocity`: seven `nonZero > pairs / 8` lines and no numerical line). **Proposal**: draw `b` for half of the pairs by `sample(a, ...)` so
that the pair is inside the lobe's support, keep the guard for that half (`nonZero > pairs / 16`) and keep the uniform pairs for the zero-zero cases.

## D2. The sample-count guard of `checkSampleConsistency` (`CL_CHECK(used > o.pairs / 4)`)

For `wo` inside the medium, a transmission-only lobe fails more than 75 % of its samples (total internal reflection has no refracted direction), so fewer than a
quarter of the samples are usable: 3 of the 42 configs. **Proposal**: require `used >= 1000` (a fixed count, enough for the pointwise comparison) in place of a fraction of the pairs.

## D3. eta 1.0001 with refract 1 or 2: the transmission lobe is a spike a uniform-sphere estimator cannot see

With an index of 1.0001 every microfacet normal refracts `wo` to within about 1e-4 of `-wo` (my analysis of Snell's law, not measured on its own): the
transmission lobe is a spike of angular width of the order of eta - 1, whatever alpha is. The uniform-sphere integral of its density therefore sees almost none
of it. `Bsdf_microfacet_PdfNorm`, the total `integral + P(failed) + P(delta)` against 1: for refract 1 it reads 0.0 to 0.0095 (GGX) and 0.0 to 0.49 (Beckmann);
for refract 2 it reads 0.0500 to 0.0518 (GGX, and Beckmann at `wo.z` +0.95, +0.5, -0.6 with 0.0500 to 0.0534: exactly the reflection choice probability 0.05) and
0.23 to 0.58 at the larger Beckmann alphas. `Bsdf_microfacet_Energy`'s cross-estimator fails the same way (sampled albedo near 0.95, uniform integral near 0.05 or
0), and `Bsdf_microfacet_ChiSquare` reads chi-square values of 1e8 to 1e9 against a critical value of 11 to 64 because the spike is in one cell of an 8 x 8 grid
and its expectation, from the midpoint rule, is wrong by orders of magnitude. All 31 PdfNorm, 32 Energy and 28 of the 36 ChiSquare failures are eta 1.0001
rows. One further row (Beckmann, refract 2, eta 1.0001) fails `sampled albedo <= 1 + 3 sigma`: the Beckmann fallback (D cos sampling, not visible normals)
has an unbounded weight `F G2 (wo . h) / (wo.z h.z)`, so its sample variance estimate is not reliable on that spike. **Proposal**: run the uniform-sphere
estimators, the chi-square and the `<= 1 + 3 sigma` bound for eta 1.0001 only through the sampled estimator (`crossEstimators = false`, `chiSquare = false`),
the way alpha 0.05 already is, and keep the eta 1.0001 rows for reciprocity, sample consistency and the reverse density (all of which pass). Or replace 1.0001
by 1.05, which is index-matched enough for the "near-unity" intent and resolvable by a 48 x 48 midpoint rule.

## D4. `checkChiSquare`: the expectation of the failed cell comes from a quadrature error

`eFail = (1 - integral of the density) * N` is the expected count of failed samples. For a lobe that never fails the true value is 0, but the midpoint rule
integrates the density with an error of the order of 5e-4 where the density has a discontinuity inside a cell (the critical cone of a refraction: the
transmitted directions of a ray entering glass stop at cos = 0.745, inside the cell [-0.75, -0.5]), so `eFail` is about 200 with N = 400000 and the observed count
is 0: the cell alone contributes `200^2 / 200 = 200` to the statistic. Observed: refract 1, alpha 0.9, eta 1.5, `wo.z` 0.95: chi-square 227 against a critical value
of 34.7, the density integral 0.99946, a sampled failure fraction of exactly 0 (measured separately). 8 of the 36 ChiSquare failures are this (eta 1.5 rows).
**Proposal**: estimate the quadrature error by integrating with 48 and with 96 sub-cells per cell and drop the failed cell when `eFail` is below
`4 * |difference| * N` (the expectation is then within the quadrature's own uncertainty); the observed counts of the other cells stay exact.

## Counts, from the logs

| Test | Configs run | Red | Of which D1/D2 | Of which D3 | Of which D4 |
|---|---|---|---|---|---|
| `Bsdf_microfacet_Reciprocity` | 42 | 7 guards | 7 | 0 | 0 |
| `Bsdf_microfacet_SamplePdf` | 42 | 3 guards | 3 | 0 | 0 |
| `Bsdf_microfacet_PdfNorm` | 100 rows | 31 | 0 | 31 | 0 |
| `Bsdf_microfacet_Energy` | 150 rows | 32 | 0 | 32 | 0 |
| `Bsdf_microfacet_ChiSquare` | 97 rows | 36 | 0 | 28 | 8 |

## Decision, 2026-11-25, and the result of applying it (Linux, GCC 13, `-j2`)

The maintainer approved D1 to D4 (D2 narrowed, D3 option (a): eta 1.0001 with refract 1 or 2 runs the sampled estimator only, no 1 + 3 sigma bound, no
cross-estimator, no chi-square; D4 as proposed, each dropped cell logged). Applied as approved, nothing else changed: of the five red microfacet tests,
Reciprocity, Energy and PdfNorm are now green. **Two tests stay red, for two defects of the same kind that D1 to D4 did not cover (new, to decide):**

### D5. `Bsdf_microfacet_SamplePdf`: the 1000-sample floor is not met by two alpha 0.05 rows with wo inside the medium

Measured usable samples (of 20000 pairs; the rest are total internal reflection): ggx refract 1 eta 1.5 alpha 0.05 wo.z -0.60: **303**; beckmann refract 1
eta 1.5 alpha 0.05 wo.z -0.60: **0**. At that wo the critical angle needs a microfacet tilted by 11.4 degrees or more, and a Beckmann lobe of alpha 0.05 has a
mass of exp(-(tan 11.4 deg / 0.05)^2) = exp(-16) there (a GGX tail is heavier), so zero is the physics, not a defect of the sampler. Every other row passes
(next lowest: beckmann alpha 0.3, 2601, floor was pairs / 4 = 5000 before D2). **Options**: (a) the floor is `used >= 1` where the expected usable fraction is
below 1 / 20 (alpha 0.05, wo inside) and that row's pointwise comparison is simply vacuous for beckmann (0 samples compared); (b) skip the alpha 0.05 rows with
wo inside for refract 1 in SamplePdf only; (c) raise the sample count for those rows (a Beckmann alpha 0.05 row would need ~1e9 pairs: not practical).
Recommendation: (b), stated in the test header.

### D6. `Bsdf_microfacet_ChiSquare`: the 48 x 48 midpoint cell integral is itself not accurate enough for five rows

Five rows remain over the critical value with the 48-sub-cell expectation: ggx refract 1 alpha 0.9 wo.z +0.12 (32.3 vs 29.8), ggx refract 2 alpha 0.3 wo.z +0.95
(108.6 vs 80.2), ggx refract 2 alpha 0.9 wo.z +0.12 (89.7 vs 77.5), beckmann refract 1 alpha 0.3 wo.z +0.95 (154.9 vs 24.5), beckmann refract 2 alpha 0.3 wo.z +0.95
(226.6 vs 64.0). A scratch run (Linux tree only, reverted) with 128 sub-cells per cell: the last four **pass** (45.2, 30.6, 18.5, 57.1; the density integral
moves 1.00209 to 1.00029 and 1.00195 to 1.00025: the 48 rule over- or under-counts peak cells by 0.1 to 0.2%), so they are quadrature, as D4 was. The first row does
not become green with a finer rule (85.4 at 128: the density integrates to 0.9998 while no sample ever fails, so the failed cell expects 81 and observes 0; the
D4 rule does not fire because 128 and 256 sub-cells agree, the 0.0002 is a converged integral, not a quadrature residue). With the 128-sub-cell rule ggx refract 1
alpha 0.3 wo.z +0.12 also tips (32.1 vs 31.4). **Options**: (a) 128 sub-cells for the cell integrals (cost: about 7x per row, ChiSquare from 13 s to about 90 s on
Linux) and the D4 rule stays; (b) keep 48 and drop the cells whose expectation differs between 48 and 96 sub-cells by more than 3 sigma of the cell count
(logged), the same idea as D4 applied to every cell; (c) either plus an explanation of the 0.0002 deficit of ggx refract 1 alpha 0.9 wo.z +0.12 (0.9998 integral,
failed cell 81 against 0 observed): measure it separately before deciding whether it is the density or the grazing quadrature. No test was changed for D5 or D6.

## D5 applied, D6 measured (2026-11-25)

D5 option (b) applied (maintainer-approved): `Bsdf_microfacet_SamplePdf` skips the refract 1, alpha 0.05, eta 1.5 rows with wo inside (wo.z -0.60, both distributions);
reason in the header of `test_bsdf_microfacet.cpp`. Linux `-L closures`: 52 of 53, only `Bsdf_microfacet_ChiSquare` red.

D6 measurement (scratch harness in the Linux tree only, reverted; nothing staged), ggx refract 1 eta 1.5, wo.z +0.12, IMPORTANCE mode, 8e6 samples:
- The density integrates to 1.000000 (alpha 0.3) and 1.000001 (alpha 0.9) on a fine grid (8192 z x 512 phi), 1.000011 and 1.00004 on 32768 x 256; the coarse
  2048 x 256 gives 1.00022 and 1.00084. The 0.0002 is not in the density.
- Sampler against density, per band of z (sampled fraction / integrated pdf), alpha 0.9: [-0.9,-0.75) 0.002569/0.002611, [-0.75,-0.5) 0.121429/0.121277,
  [-0.5,-0.25) 0.434572/0.434231, [-0.25,-0.1) 0.389147/0.389606, [-0.1,-0.01) 0.052282/0.052276; alpha 0.3: 0.036694/0.036684, 0.535652/0.535247, 0.301497/0.301716,
  0.112644/0.112884, 0.013512/0.013470. Largest deviation 2.7 sigma, all bands within 1e-3 relative; no sample with z > 0 (none above the cutoff), no sample whose
  evaluated pdf is 0, mean(pdf evaluate / pdf sampled) = 1.000000. No directions the sampler produces that the pdf misses.
- The cause of the deficit: the density has a hard cutoff at **z = -wo.z / eta = -0.0800 exactly** (the microfacet hemisphere boundary m.z = 0: a wi with
  wi.z > -wo.z/eta has no microfacet normal), with a finite jump of 1.146 (alpha 0.3) and 4.45 (alpha 0.9) (GGX's D is alpha^2 / pi at the horizon, not 0, and
  the visible-normal density is non-zero there for a grazing wo); Beckmann's D is 0 there, so no jump. A midpoint rule over a step of height J and azimuthal
  integral I_edge = 2.59 (alpha 0.9) or 0.665 (alpha 0.3) errs by (offset of the edge from the sub-cell midpoint, in sub-cells) x dz x I_edge. Predicted against
  measured: 48 sub-cells (edge at sub-cell 176.64, counted 0.36 too many): +0.00486 / +0.00125, measured integrals 1.00486 / 1.00125; 128 sub-cells (edge at 471.04,
  0.04 missed): -0.00020 / -0.00005, measured 0.99980 / 0.99995 (log: failed cell expects 81 / 21, observes 0). 48 vs 96 sub-cells (the D4 rule) differ here (the
  edge sits at 353.28 at 96), but 128 vs 256 sub-cells share the same absolute offset (471.04 = 942.08 / 2), so a rule that compares a rule with its double
  can be blind to it.
Conclusion: a quadrature artifact of a genuine analytic discontinuity of the GGX density; the density, the sampler and the pdf agree; no code change proposed.
The leaning option (b) (keep 48, leave out of the statistic every cell whose 48 and 96 sub-cell expectations differ by more than 3 sigma of its count, logged like
D4) handles this row: at 48 the cell containing the edge (expectation ~1.7e5 of 4e5 samples) moves by 0.0056 x 4e5 = 2200 counts between 48 and 96, about 5 sigma
(sigma ~ 420), so the cell is dropped and logged; the failed cell is covered by the same rule. Not applied: waiting for the maintainer.

## D6 option (d): measurement, and a result that stops the plan (2026-11-25; scratch only, nothing applied)

Scratch harness in the Linux tree (reverted). Error of the test's midpoint total against a truth integral (uniform in theta, the ring split at the edge, 1500 x 48 points per
piece), for the five red rows, at 48, 96, 192, 384 sub-cells per cell:
- **Explained by a closed-form edge (2 rows)**: ggx refract 1 alpha 0.9 wo.z +0.12 (+0.004862, -0.001891, +0.001486, -0.000203) and ggx refract 2 alpha 0.9 wo.z +0.12
  (+0.002365, -0.000920, +0.000723, -0.000099): the ring z = -wo.z / eta_rel = -0.08 (m.z = 0). Model: (1[frac > 1/2] - frac) x dz x I, frac the edge's offset in sub-cells
  (176.64, 353.28, 706.56, 1413.12 -> 0.64, 0.28, 0.56, 0.12), I = 2.59 and 1.26 (the azimuthal integral of the density at the edge): predicted +0.00486, -0.00189,
  +0.001484, -0.000202 and +0.00236, -0.00092, +0.000722, -0.000098: the measured values, to the digit. The jump scan finds a ring at exactly -0.08000 for ggx refract 1 and 2 with
  wo.z +0.12 (both alphas), and at 0.90000 = 0.6 x 1.5 for wo.z -0.60 (the table's formula for wo inside), and the ring at 0 (the horizon of wi, a cell boundary).
- **Not explained by an edge (3 rows)**: ggx refract 2 alpha 0.3 wo.z +0.95, beckmann refract 1 and refract 2 alpha 0.3 wo.z +0.95. No ring jump at -0.6333 (scan: none; Beckmann's
  D is 0 at the horizon, GGX's jump at wo.z +0.95 is below 1e-3 of the peak). Their total error is +0.000658 / +0.002089 / +0.001984 at 48 sub-cells and halves-and-more with
  each doubling (beckmann refract 1: +0.002089, +0.000518, +0.000129, +0.000032: x4 per doubling, h^2 at the totals; the per-cell residual -77, -39, -18, -7 counts: x2.0, 2.2, 2.4
  tending to x2.83 = h^1.5) and sits in the **pole cell** (iz 0, ip 0, 1, 2, 7: -73 to -80 counts against 150 to 660 expected, 5 to 6 sigma). Cause: a lobe is smooth in the direction, near
  a pole a function of sqrt(1 - |z|), and the midpoint rule in z over a square root is O(h^1.5) per sector. It is a coordinate singularity of the test's grid, not an edge.
  The remedy is closed-form too: integrate the two cells that touch a pole uniformly in theta (weight sin theta) instead of z.
- Also found, not an edge in z: Beckmann transmission has a support boundary on the curve k = wo.m = 0 (its D cos fallback density has no factor k, so it does not go to 0 there):
  the density is 0 below z = -wo.z / eta - sqrt(1 - 1/eta^2) sqrt(1 - wo.z^2) = -0.978831 (wo.z +0.5) and 0.538 above it, at the azimuth of the curve's extremum. Not a ring, so the z split does
  not touch it; the rows with it (wo.z +0.5, +0.95, +0.12) pass with or without the pole change.
Prototype (not applied; scratch copies `scratchpad/bsdf_checks_d.h` and `scratchpad/test_bsdf_microfacet_d.cpp`, which also hold the jump scan to be removed): `Options::zEdges`, a helper
`microfacetEdges(c, woZ)` with the derivation, and `integrateCells()` (pole cells in theta, sub-cells split at the edges, the D4 96-sub-cell pass through the same function, observed counts and every cell kept).
Result on Linux, whole `Bsdf_microfacet_ChiSquare` (68 rows): **passes**, 37.8 s with the scan (the largest chi2 / critical 0.77); the five rows, chi-square before (48-sub-cell plain rule) -> after,
critical in brackets: ggx r1 a0.9 +0.12 32.3 -> 4.8 (29.8); ggx r2 a0.3 +0.95 108.6 -> 42.1 (80.2); ggx r2 a0.9 +0.12 89.7 -> 30.3 (77.5); beckmann r1 a0.3 +0.95 154.9 -> 10.0 (24.5);
beckmann r2 a0.3 +0.95 226.6 -> 36.0 (64.0); and ggx r1 a0.3 +0.12 (which tips at 128 sub-cells) 12.1 (29.8). D4's failed-cell rule still fires twice (ggx r1 a0.3 +0.12 and a0.9 +0.50: expected 36 and 11 against 4 x
118 and 4 x 34) and is the only thing left out of the statistic.
Decision needed: the pole-cell integration is a change to the quadrature beyond the approved edge splitting, because three of the five rows are not explained by an edge.

## Resolution of D6, 2026-11-25: approved and applied

The maintainer approved (d), edge splitting, plus theta integration of the two pole cells (replacing option (b)). Applied: `Options::zEdges`, `microfacetEdges()` in
`test_bsdf_microfacet.cpp` (the derivation of each edge in its comment), `integrateCells()` in `bsdf_checks.h` (pole cells in theta, sub-cells split at the edges; the D4 96-sub-cell
pass goes through it); the observed counts and every cell stay in the statistic; D4's failed-cell rule is unchanged and logs its drops (two rows on Linux: ggx refract 1 alpha 0.3 wo.z +0.12
and ggx refract 1 alpha 0.9 wo.z +0.50). Linux: `Bsdf_microfacet_ChiSquare` passes (13.0 s). D1 to D6 are all resolved.

## D7 (new, 2026-11-25): the chi-square family-wise level; one row fails on macOS with its fixed seed

After D1 to D6 the macOS run is 52 of 53: `Bsdf_microfacet_ChiSquare`, beckmann refract 2 eta 1.5 alpha 0.3 wo.z +0.50, chi-square 55.0 (df 23, critical 49.8, level 0.001). Linux passes the same row at 20.9.
The sample realisations differ by platform (all 68 rows' values differ between hosts; densities identical). Evidence that it is a tail event, not a defect: the 192 sub-cell expectation moves the value to 51.35 only (the 48 sub-cell
rule is accurate to 0.15 sigma per cell); 600 other seeds on Linux: mean 23.02, sd 6.43, 0 exceedances of 49.8 (0.6 expected); 61 on macOS: mean 22.31, 0 exceedances, no cell with a systematic bias.
The seed's own tail probability is about 5e-4. The test makes 68 chi-square decisions at 0.001: a 6.6% chance per platform that one fails by chance.
Options (none applied; the rules forbid reseeding and widening): (a) a family-wise correction, each check at 0.001 / 68 = 1.5e-5 (critical value for df 23 about 63 by Wilson-Hilferty with z = 4.17, 51.35 and 55.0 pass), stated in the header and in the tolerance table:
it is a change of the threshold, so it needs your approval; (b) 1,000,000 samples per row (the statistic has the same level, the realisation changes: a re-seed in effect, not recommended); (c) keep the test and
treat the one red macOS row as a known per-platform failure (not recommended: a test that fails every time on a platform).
Recommendation: (a).

## Resolution of D7, 2026-11-25: approved (a) and applied

Per-row level = Sidak over the n chi-square rows the test runs (counted at run time: 68), 1.471e-05, critical value for df 23 63.2; the macOS row reads 55.0 (51.35 at 192 sub-cells) and passes. Linux 53 of 53, macOS 53 of 53 (closures),
27 of 27 (link|common), 45 of 45 (architecture). D1 to D7 are all resolved.
