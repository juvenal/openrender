# BSDF sources and licence record (spec 035, FR-012, research R7, tasks T044 to T046)

Nothing under `src/libshader/closures/bsdf*.{h,cpp}` is adapted from outside code. Every function is **written from the published papers** listed
here, by the implementer, in this repository, so there is no third-party licence to check against LGPL-2.1. If code is ever adapted, its source and
licence go in this file first and the maintainer decides (a PAUSE). The classic `diffuse()`, `specular()` and `phong()` builtins are not rerouted
through this library (FR-012): their bytes and images are unchanged.

## Per function

| Function (file) | Written from |
|---|---|
| `orthonormalBasis` (`bsdfMath.cpp`) | Duff, Burgess, Christensen, Hery, Kensler, Liani, Villemin, "Building an Orthonormal Basis, Revisited", JCGT 6(1), 2017 |
| `microfacetD`, `ggxD`, `beckmannD` | Walter, Marschner, Li, Torrance, "Microfacet Models for Refraction through Rough Surfaces", EGSR 2007 (GGX and Beckmann `D`); the GGX form `alpha^2 / (pi (cos^2 (alpha^2 - 1) + 1)^2)` is the same distribution written without the tangent |
| `smithLambda` (GGX) | Heitz, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs", JCGT 3(2), 2014 (`Lambda = (sqrt(1 + alpha^2 tan^2) - 1) / 2`; the height-correlated `G2 = 1 / (1 + Lambda_o + Lambda_i)`) |
| `smithLambda` (Beckmann) | Walter et al. 2007, the rational approximation of Smith's `G1` (eq. 27), turned into `Lambda = 1 / G1 - 1` |
| `sampleGgxVisibleNormal` | Heitz, "Sampling the GGX Distribution of Visible Normals", JCGT 7(4), 2018 |
| `sampleBeckmannNormal`, `visibleNormalPdf` (Beckmann) | **The fallback of research R7**: a normal sampled from `D(h) cos(theta_h)`, density `D(h) h.z`. See "Decisions" |
| `fresnel`, `fresnelDielectric` | The exact unpolarised dielectric Fresnel equations (the average of the s and p reflectances), as in any optics text; Cook and Torrance 1982 is the usual graphics reference |
| `refractThrough`, the transmission branch of `roughTerms` | Walter et al. 2007: the generalised half vector `h_t = -(eta_o o + eta_i i)`, the BTDF of eq. 21 and the change of variables `dw_h / dw_i = eta_i^2 |i . h| / (eta_i i . h + eta_o o . h)^2` |
| the radiance-mode factor `(eta_wo / eta_wi)^2` | Veach, "Robust Monte Carlo Methods for Light Transport Simulation", PhD thesis, Stanford 1997, section 5.2 (non-symmetric scattering due to refraction); with Walter's `eta_o^2` numerator it makes `f_importance(wo, wi) == f_radiance(wi, wo)` |
| `sampleCosineHemisphere` | Malley's method (a uniformly sampled disk projected onto the hemisphere), as in Pharr, Jakob, Humphreys, "Physically Based Rendering" |
| `orenNayarFactor` | Oren and Nayar, "Generalization of Lambert's Reflectance Model", SIGGRAPH 1994: the qualitative model, `A = 1 - sigma^2 / (2 (sigma^2 + 0.33))`, `B = 0.45 sigma^2 / (sigma^2 + 0.09)`, with the `max(0, cos(phi_i - phi_o)) sin(alpha) tan(beta)` term, as OSL's `oren_nayar` closure uses it |
| `powerHeuristic`, `balanceHeuristic` | Veach and Guibas, "Optimally Combining Sampling Techniques for Monte Carlo Rendering", SIGGRAPH 1995 (beta = 2 for the power heuristic) |
| the lobe selection `mean(weight) * albedo(wo)` (`bsdf.cpp`) | The weight-times-albedo choice of the contract (convention 4); the mixture density is the one-sample model of Veach and Guibas 1995 |

## Decisions recorded at the Phase 2 checkpoint

1. **Beckmann sampling uses the fallback (maintainer approved, 2026-11-24).** GGX samples the distribution of visible normals (Heitz 2018). Beckmann
   samples `D(h) cos(theta_h)` instead of visible normals (Heitz and d'Eon 2014 is not implemented): the interface is the same, only the variance of
   `f cos / pdf` differs (it is not 1 for a reflection lobe's `G2 / G1`). Reported at the checkpoint for the maintainer.
2. **`microfacet` `refract == 2` is unverified.** Searched on this machine, locally and only there (no web lookup): `/opt/homebrew/include`,
   `/usr/local/include` and the vcpkg include directory (`~/.cache/vcpkg-installed/openrender/arm64-osx-openrender/include`) hold no `OSL/` directory,
   no `stdosl.h` and no `oslclosure.h`; `/opt/homebrew/opt` and `/opt/homebrew/Cellar` hold no `osl` or `openshadinglanguage` formula; `docs/references/` holds the RISpec and the NSI documents and
   papers only. So the meaning of the value 2 is **implemented as contracts/closure-abi.md writes it**: `refract` 0 is reflection (with the Fresnel
   term, or none for `eta` 0), 1 is transmission (with `1 - F`), 2 is both (reflection `F`, transmission `1 - F`, the lobe choice between them made by a
   probability that is a clamp of the Fresnel term at `wo`). It is a registry detail under the unstable 1.x; spec 049 compares it with `stdosl.h`.
3. **The lobe-selection estimates are not exact albedos.** `albedo()` is for lobe selection only: positive and at most 1 (floor `kBsdfAlbedoFloor`
   0.01 so a lobe that contributes is always chosen with a positive probability): `diffuse` and `oren_nayar` 1, a mirror or a reflection-only
   microfacet `F(cos theta_o)` (1 for `eta` 0), a refraction `1 - F`, a both-sided microfacet 1. The estimators of the tests fix the choice only where
   the albedo is exact (a diffuse lobe, a mirror of `eta` 0).
4. **Float versus double.** The math is double precision inside and cast to float at the answer, so a symmetric formula is symmetric to far better
   than the float tolerance of the tests.
