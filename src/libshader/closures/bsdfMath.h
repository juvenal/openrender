/**
 * Project: openRender
 *
 * File: bsdfMath.h
 *
 * Description:
 *   The shared BRDF math library (spec 035, D19, FR-012): orthonormal frames, GGX and Beckmann D and Smith lambda, visible-normal sampling,
 *   the dielectric Fresnel term, microfacet refraction, Oren-Nayar, cosine sampling and the power-heuristic MIS weight. Pure functions: no
 *   state, no allocation, no random numbers (the caller supplies them). Written from the papers listed in specs/035-closure-tree-bsdf/
 *   bsdf-sources.md; nothing is adapted from outside code. Classic diffuse(), specular() and phong() are not rerouted through it.
 *
 *   Internally everything is double: the inputs are floats, and a formula of a few dozen operations (D, G, a Fresnel term, a division by two
 *   cosines, the denominator of a refraction) is then symmetric in its arguments to far better than the float tolerance of the tests, which
 *   keeps reciprocity and the sample/density consistency a property of the formulas and not of the rounding.
 *
 *   Conventions: a local frame has z as the normal; wo and wi point away from the surface; eta is the index of refraction of the medium
 *   BELOW the surface over the one ABOVE it (n_t / n_i for a ray arriving from above); a ray arriving from below sees 1 / eta.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef BSDFMATH_H
#define BSDFMATH_H

namespace openrender::shading::bsdf {

    // Below this alpha a microfacet lobe is the delta lobe it converges to (the edge case "roughness at or near zero")
    inline constexpr float kMicrofacetDeltaAlpha = 1e-4f;

    enum class Distribution { Ggx, Beckmann };

    // "beckmann" is Beckmann; anything else (including "ggx" and a null name) is GGX
    Distribution distributionFromName(const char *name);

    // The microfacet normal distribution D(m) as a function of the cosine of the angle between m and the macro normal (0 below the horizon)
    float ggxD(float alpha, float cosThetaM);
    float beckmannD(float alpha, float cosThetaM);
    double microfacetD(Distribution d, double alpha, double cosThetaM);

    // Smith's Lambda of a direction with the given cosine to the macro normal; G1 = 1 / (1 + Lambda), the height-correlated G2 = 1 / (1 + Lambda_o + Lambda_i)
    double smithLambda(Distribution d, double alpha, double cosTheta);

    // The exact dielectric Fresnel reflectance (unpolarised). eta = n_t / n_i for a positive cosine; a negative cosine is a ray arriving from the
    // other side (the index is inverted). eta <= 0 means "no Fresnel term": 1. Total internal reflection gives 1.
    float fresnelDielectric(float cosThetaI, float eta);
    double fresnel(double cosThetaI, double eta);

    // An orthonormal basis (t, b, n) from a unit normal (Duff et al. 2017, "Building an Orthonormal Basis, Revisited")
    void orthonormalBasis(const double n[3], double t[3], double b[3]);

    // A microfacet normal sampled from the distribution of VISIBLE normals of a lobe seen from wo (wo.z > 0, unit): GGX by Heitz 2018. The
    // returned h is in the upper hemisphere and its density in h is G1(wo) max(0, wo . h) D(h) / wo.z.
    void sampleGgxVisibleNormal(double alpha, const double wo[3], double u1, double u2, double h[3]);
    // Beckmann: the fallback of research R7, a normal sampled from D(h) cos(theta_h) (density D(h) h.z in h); the same interface, more variance
    void sampleBeckmannNormal(double alpha, double u1, double u2, double h[3]);

    // The density in h of the two samplers above
    double visibleNormalPdf(Distribution d, double alpha, double cosThetaO, double woDotH, double cosThetaH);

    // Refracts the unit vector wo (the ray's side, wo . h > 0) through the microfacet normal h into a direction on the other side: etaRel = the
    // index of wi's medium over wo's. Returns false for total internal reflection.
    bool refractThrough(const double wo[3], const double h[3], double etaRel, double wi[3]);

    // Cosine-hemisphere sampling (Malley's method): a unit vector with z > 0 of density z / pi
    void sampleCosineHemisphere(double u1, double u2, double w[3]);

    // The Oren-Nayar qualitative model (Oren and Nayar 1994), as OSL's oren_nayar uses it: the bracket (A + B max(0, cos(phi_i - phi_o)) sin(alpha)
    // tan(beta)) that multiplies 1/pi; sigma is the roughness in radians. wo.z and wi.z must be positive. sigma 0 gives exactly 1.
    double orenNayarFactor(double sigma, const double wo[3], const double wi[3]);

    // Veach's power heuristic (beta = 2) for n_f samples of density p_f against n_g of p_g, and the balance heuristic
    double powerHeuristic(double nf, double pf, double ng, double pg);
    double balanceHeuristic(double nf, double pf, double ng, double pg);

} // namespace openrender::shading::bsdf

#endif
