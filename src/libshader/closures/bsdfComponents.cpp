/**
 * Project: openRender
 *
 * File: bsdfComponents.cpp
 *
 * Description:
 *   The five built-in BSDF components (spec 035, bsdfComponents.h), written from the papers listed in specs/035-closure-tree-bsdf/bsdf-sources.md.
 *   Everything inside is double precision; the answers are cast to float at the end. Every function is pure.
 *
 *   Conventions (contracts/closure-abi.md): wo and wi point away from the surface; f excludes |cos(theta_i)|; densities are per steradian; every
 *   answer fills pdfForward = p(wi | wo) and pdfReverse = p(wo | wi) under the same transport mode; a delta sample reports pdfForward and pdfReverse
 *   as DISCRETE probabilities (1 for a component on its own); below the horizon or failed, f and the densities are 0 and a sample carries
 *   OR_BSDF_FAILED. A lobe that reflects only is one-sided: it gives nothing for wo on or below its horizon. A lobe that transmits serves wo on
 *   either side: wo below the horizon is the ray inside the medium (z is flipped and the indices swapped).
 *
 *   Radiance transport multiplies a transmissive lobe by (eta_wo / eta_wi)^2, eta_wo being the index of the medium wo is in; importance transport
 *   does not. With Walter et al. 2007's BTDF this is the numerator eta_wo^2 (radiance) against eta_wi^2 (importance), which makes
 *   f_importance(wo, wi) == f_radiance(wi, wo): the adjoint relation the reciprocity test checks.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "bsdfComponents.h"

#include <cmath>
#include <math.h>
#include <string.h>

#include "bsdfMath.h"
#include "closureBuiltins.h"

namespace openrender::shading {

    namespace {
        using namespace bsdf;

        constexpr double kInvPi = 1.0 / 3.14159265358979323846;

        void zeroEval(OrBsdfEval *o, uint32_t flags = 0) {
            memset(o, 0, sizeof(*o));
            o->numChannels = 3;
            o->flags = flags;
        }
        void failSample(OrBsdfSample *o) {
            memset(o, 0, sizeof(*o));
            o->numChannels = 3;
            o->flags = OR_BSDF_FAILED;
            o->eta = 1.0f;
        }
        void setWhite(float *f, double v) {
            f[0] = f[1] = f[2] = (float)v;
            f[3] = 0.0f;
        }
        void copyToSample(const OrBsdfEval &e, OrBsdfSample *o) {
            memcpy(o->f, e.f, sizeof(o->f));
            o->numChannels = e.numChannels;
            o->pdfForward = e.pdfForward;
            o->pdfReverse = e.pdfReverse;
            o->flags = e.flags;
        }
        bool usable(const float w[3]) { return w[2] == w[2] && w[2] != 0.0f; }
        double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

        // the albedo estimates are for lobe selection only: positive, at most 1
        float albedoFrom(double x) { return (float)clampd(x, kBsdfAlbedoFloor, 1.0); }

        // ---- diffuse and oren_nayar ----

        void diffuseEvaluate(const void *, const float wo[3], const float wi[3], OrTransportMode, OrBsdfEval *out) {
            zeroEval(out);
            if (!(wo[2] > 0.0f) || !(wi[2] > 0.0f))
                return;
            setWhite(out->f, kInvPi);
            out->pdfForward = (float)(wi[2] * kInvPi);
            out->pdfReverse = (float)(wo[2] * kInvPi);
            out->flags = OR_BSDF_REFLECTION | OR_BSDF_DIFFUSE;
        }
        void cosineSample(void (*evaluate)(const void *, const float[3], const float[3], OrTransportMode, OrBsdfEval *), const void *block, const float wo[3], float u1, float u2, OrTransportMode mode,
                          OrBsdfSample *out) {
            if (!(wo[2] > 0.0f)) {
                failSample(out);
                return;
            }
            double w[3];
            sampleCosineHemisphere(u1, u2, w);
            const float wi[3] = {(float)w[0], (float)w[1], (float)w[2]};
            OrBsdfEval e;
            evaluate(block, wo, wi, mode, &e);
            memset(out, 0, sizeof(*out));
            if (!(e.pdfForward > 0.0f) || !(e.f[0] > 0.0f)) {
                failSample(out);
                return;
            }
            copyToSample(e, out);
            memcpy(out->wi, wi, sizeof(wi));
            out->eta = 1.0f;
        }
        void diffusePdf(const void *block, const float wo[3], const float wi[3], OrTransportMode m, OrBsdfEval *out) {
            diffuseEvaluate(block, wo, wi, m, out);
            memset(out->f, 0, sizeof(out->f));
        }
        void diffuseSample(const void *block, const float wo[3], float u1, float u2, OrTransportMode m, OrBsdfSample *out) { cosineSample(diffuseEvaluate, block, wo, u1, u2, m, out); }
        float diffuseAlbedo(const void *, const float[3]) { return 1.0f; }

        void orenNayarEvaluate(const void *block, const float wo[3], const float wi[3], OrTransportMode, OrBsdfEval *out) {
            const ClosureOrenNayarBlock *b = (const ClosureOrenNayarBlock *)block;
            zeroEval(out);
            if (!(wo[2] > 0.0f) || !(wi[2] > 0.0f))
                return;
            const double o[3] = {wo[0], wo[1], wo[2]}, i[3] = {wi[0], wi[1], wi[2]};
            setWhite(out->f, kInvPi * orenNayarFactor(b->sigma, o, i));
            out->pdfForward = (float)(wi[2] * kInvPi);
            out->pdfReverse = (float)(wo[2] * kInvPi);
            out->flags = OR_BSDF_REFLECTION | OR_BSDF_DIFFUSE;
        }
        void orenNayarPdf(const void *block, const float wo[3], const float wi[3], OrTransportMode m, OrBsdfEval *out) {
            orenNayarEvaluate(block, wo, wi, m, out);
            memset(out->f, 0, sizeof(out->f));
        }
        void orenNayarSample(const void *block, const float wo[3], float u1, float u2, OrTransportMode m, OrBsdfSample *out) { cosineSample(orenNayarEvaluate, block, wo, u1, u2, m, out); }

        // ---- the delta lobes ----

        // a mirror: F / |cos|, the reflected direction, the discrete probability 1
        bool mirrorSample(double eta, const float wo[3], OrBsdfSample *out) {
            if (!(wo[2] > 0.0f)) {
                failSample(out);
                return false;
            }
            memset(out, 0, sizeof(*out));
            out->numChannels = 3;
            out->wi[0] = -wo[0];
            out->wi[1] = -wo[1];
            out->wi[2] = wo[2];
            setWhite(out->f, fresnel(wo[2], eta) / (double)wo[2]);
            out->pdfForward = out->pdfReverse = 1.0f;
            out->flags = OR_BSDF_DELTA | OR_BSDF_REFLECTION | OR_BSDF_SPECULAR;
            out->eta = 1.0f;
            return true;
        }

        // the Snell direction of wo through a flat interface and the transmitted fraction (1 - F), in radiance mode times (eta_wo / eta_wi)^2
        bool snellSample(double eta, const float wo[3], OrTransportMode mode, OrBsdfSample *out) {
            if (!(eta > 0.0))
                eta = 1.0;
            if (!usable(wo)) {
                failSample(out);
                return false;
            }
            const bool inside = wo[2] < 0.0f;
            const double etaWo = inside ? eta : 1.0, etaWi = inside ? 1.0 : eta;
            const double ratio = etaWo / etaWi; // the tangential part of wi is -(the tangential part of wo) * ratio
            const double sin2 = ((double)wo[0] * wo[0] + (double)wo[1] * wo[1]) * ratio * ratio;
            if (sin2 >= 1.0) { // total internal reflection: a refraction-only lobe has nothing to give
                failSample(out);
                return false;
            }
            const double cosT = sqrt(1.0 - sin2);
            memset(out, 0, sizeof(*out));
            out->numChannels = 3;
            out->wi[0] = (float)(-wo[0] * ratio);
            out->wi[1] = (float)(-wo[1] * ratio);
            out->wi[2] = (float)(inside ? cosT : -cosT);
            const double F = fresnel(wo[2], eta);
            double v = (1.0 - F) / cosT;
            if (mode == OR_TRANSPORT_RADIANCE)
                v *= ratio * ratio;
            setWhite(out->f, v);
            out->pdfForward = out->pdfReverse = 1.0f;
            out->flags = OR_BSDF_DELTA | OR_BSDF_TRANSMISSION | OR_BSDF_SPECULAR;
            out->eta = (float)(etaWi / etaWo);
            return true;
        }

        void deltaEvaluate(const void *, const float[3], const float[3], OrTransportMode, OrBsdfEval *out, uint32_t kind) {
            zeroEval(out);
            out->flags = OR_BSDF_HAS_DELTA | kind;
        }

        void reflectionEvaluate(const void *b, const float wo[3], const float wi[3], OrTransportMode m, OrBsdfEval *out) { deltaEvaluate(b, wo, wi, m, out, OR_BSDF_REFLECTION | OR_BSDF_SPECULAR); }
        void reflectionSample(const void *block, const float wo[3], float, float, OrTransportMode, OrBsdfSample *out) { mirrorSample(((const ClosureReflectionBlock *)block)->eta, wo, out); }
        float reflectionAlbedo(const void *block, const float wo[3]) {
            const double eta = ((const ClosureReflectionBlock *)block)->eta;
            if (!(eta > 0.0))
                return 1.0f;
            return wo[2] > 0.0f ? albedoFrom(fresnel(wo[2], eta)) : kBsdfAlbedoFloor;
        }

        void refractionEvaluate(const void *b, const float wo[3], const float wi[3], OrTransportMode m, OrBsdfEval *out) { deltaEvaluate(b, wo, wi, m, out, OR_BSDF_TRANSMISSION | OR_BSDF_SPECULAR); }
        void refractionSample(const void *block, const float wo[3], float, float, OrTransportMode m, OrBsdfSample *out) { snellSample(((const ClosureRefractionBlock *)block)->eta, wo, m, out); }
        float refractionAlbedo(const void *block, const float wo[3]) {
            double eta = ((const ClosureRefractionBlock *)block)->eta;
            if (!(eta > 0.0))
                eta = 1.0;
            return usable(wo) ? albedoFrom(1.0 - fresnel(wo[2], eta)) : kBsdfAlbedoFloor;
        }

        // ---- the microfacet lobe ----

        struct Cfg {
                Distribution dist;
                double alpha, eta;
                int refract;
        };
        Cfg configOf(const ClosureMicrofacetBlock *b) {
            Cfg c;
            c.dist = distributionFromName(b->distribution);
            c.alpha = b->alpha;
            c.eta = b->eta;
            c.refract = b->refract < 0 ? 0 : (b->refract > 2 ? 2 : b->refract);
            if (c.refract != 0 && !(c.eta > 0.0))
                c.eta = 1.0; // a refraction needs an index (the builder refuses 0 for it); 1 is a straight pass through
            return c;
        }
        bool isDeltaLobe(const Cfg &c) { return c.alpha < (double)kMicrofacetDeltaAlpha; }

        // The config space: wo' has z > 0 (a ray below the surface is flipped), etaWo and etaWi are the indices of the media of wo and wi, etaRel = the
        // other medium's index over wo's (what the Fresnel term of a ray arriving at the interface uses; 0 means "no Fresnel term")
        struct Space {
                bool flip;
                double wo[3], wi[3];
                double etaWo, etaWi, etaRel;
        };
        bool makeSpace(const Cfg &c, const float wo[3], const float wi[3], Space *s) {
            if (!usable(wo))
                return false;
            s->flip = wo[2] < 0.0f;
            if (s->flip && c.refract == 0)
                return false; // a reflection-only lobe is one-sided
            // The directions are made exactly unit length in double: the generalised half vector and Snell's law agree only for unit vectors, and near
            // the critical angle (or near index matching) a float's 1e-7 length error would be magnified in the Fresnel term of the two orders of a
            // pair, breaking the reciprocity the formulas have
            const double sign = s->flip ? -1.0 : 1.0;
            const double lo = sqrt((double)wo[0] * wo[0] + (double)wo[1] * wo[1] + (double)wo[2] * wo[2]);
            const double li = sqrt((double)wi[0] * wi[0] + (double)wi[1] * wi[1] + (double)wi[2] * wi[2]);
            if (!(lo > 0.0) || !(li > 0.0))
                return false;
            s->wo[0] = wo[0] / lo;
            s->wo[1] = wo[1] / lo;
            s->wo[2] = sign * wo[2] / lo;
            s->wi[0] = wi[0] / li;
            s->wi[1] = wi[1] / li;
            s->wi[2] = sign * wi[2] / li;
            if (c.eta > 0.0) {
                s->etaWo = s->flip ? c.eta : 1.0;
                s->etaWi = s->flip ? 1.0 : c.eta;
                s->etaRel = s->etaWi / s->etaWo;
            }
            else {
                s->etaWo = s->etaWi = 1.0;
                s->etaRel = 0.0;
            }
            return true;
        }

        double reflectProbability(const Cfg &c, const Space &s) {
            if (c.refract == 0)
                return 1.0;
            if (c.refract == 1)
                return 0.0;
            return clampd(fresnel(s.wo[2], s.etaRel), 0.05, 0.95);
        }

        struct Terms {
                double f, pdf;
                uint32_t flags;
        };

        // f and the density of wi given wo in the config space
        Terms roughTerms(const Cfg &c, const Space &s, OrTransportMode mode) {
            Terms t = {0.0, 0.0, 0};
            const double *wo = s.wo, *wi = s.wi;
            if (!(wo[2] > 0.0))
                return t;
            if (wi[2] > 0.0 && c.refract != 1) {
                // reflection
                double h[3] = {wo[0] + wi[0], wo[1] + wi[1], wo[2] + wi[2]};
                const double l = sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
                if (!(l > 1e-12))
                    return t;
                h[0] /= l;
                h[1] /= l;
                h[2] /= l;
                const double woh = wo[0] * h[0] + wo[1] * h[1] + wo[2] * h[2];
                if (!(woh > 0.0) || !(h[2] > 0.0))
                    return t;
                const double F = fresnel(woh, s.etaRel);
                const double D = microfacetD(c.dist, c.alpha, h[2]);
                const double G2 = 1.0 / (1.0 + smithLambda(c.dist, c.alpha, wo[2]) + smithLambda(c.dist, c.alpha, wi[2]));
                t.f = F * D * G2 / (4.0 * wo[2] * wi[2]);
                t.pdf = reflectProbability(c, s) * visibleNormalPdf(c.dist, c.alpha, wo[2], woh, h[2]) / (4.0 * woh);
                t.flags = OR_BSDF_REFLECTION | OR_BSDF_GLOSSY;
                return t;
            }
            if (wi[2] < 0.0 && c.refract != 0) {
                // transmission: the generalised half vector of Walter et al. 2007
                double h[3] = {-(s.etaWo * wo[0] + s.etaWi * wi[0]), -(s.etaWo * wo[1] + s.etaWi * wi[1]), -(s.etaWo * wo[2] + s.etaWi * wi[2])};
                const double l = sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
                if (!(l > 1e-14))
                    return t;
                for (double &x : h)
                    x /= l;
                if (h[2] < 0.0)
                    for (double &x : h)
                        x = -x;
                if (!(h[2] > 0.0))
                    return t;
                const double woh = wo[0] * h[0] + wo[1] * h[1] + wo[2] * h[2], wih = wi[0] * h[0] + wi[1] * h[1] + wi[2] * h[2];
                if (!(woh > 0.0) || !(wih < 0.0))
                    return t;
                const double denom = s.etaWo * woh + s.etaWi * wih, den2 = denom * denom;
                if (!(den2 > 0.0))
                    return t;
                const double F = fresnel(woh, s.etaRel);
                const double D = microfacetD(c.dist, c.alpha, h[2]);
                const double G2 = 1.0 / (1.0 + smithLambda(c.dist, c.alpha, wo[2]) + smithLambda(c.dist, c.alpha, wi[2]));
                const double etaNum = mode == OR_TRANSPORT_RADIANCE ? s.etaWo : s.etaWi;
                t.f = fabs(wih) * woh / (fabs(wi[2]) * wo[2]) * etaNum * etaNum * (1.0 - F) * G2 * D / den2;
                t.pdf = (1.0 - reflectProbability(c, s)) * visibleNormalPdf(c.dist, c.alpha, wo[2], woh, h[2]) * s.etaWi * s.etaWi * fabs(wih) / den2;
                t.flags = OR_BSDF_TRANSMISSION | OR_BSDF_GLOSSY;
                return t;
            }
            return t;
        }

        void microfacetEvaluate(const void *block, const float wo[3], const float wi[3], OrTransportMode mode, OrBsdfEval *out) {
            const Cfg c = configOf((const ClosureMicrofacetBlock *)block);
            if (isDeltaLobe(c)) {
                uint32_t kind = c.refract == 0 ? OR_BSDF_REFLECTION : (c.refract == 1 ? OR_BSDF_TRANSMISSION : (OR_BSDF_REFLECTION | OR_BSDF_TRANSMISSION));
                deltaEvaluate(block, wo, wi, mode, out, kind | OR_BSDF_SPECULAR);
                return;
            }
            zeroEval(out);
            Space fwd, rev;
            if (!usable(wi) || !makeSpace(c, wo, wi, &fwd))
                return;
            const Terms a = roughTerms(c, fwd, mode);
            if (!(a.f > 0.0) || !(a.pdf > 0.0) || !std::isfinite(a.f) || !std::isfinite(a.pdf))
                return;
            double reverse = 0.0;
            if (makeSpace(c, wi, wo, &rev)) {
                const Terms b = roughTerms(c, rev, mode);
                if (std::isfinite(b.pdf))
                    reverse = b.pdf;
            }
            setWhite(out->f, a.f);
            out->pdfForward = (float)a.pdf;
            out->pdfReverse = (float)reverse;
            out->flags = a.flags;
        }
        void microfacetPdf(const void *block, const float wo[3], const float wi[3], OrTransportMode m, OrBsdfEval *out) {
            microfacetEvaluate(block, wo, wi, m, out);
            memset(out->f, 0, sizeof(out->f));
        }

        void microfacetSample(const void *block, const float wo[3], float u1, float u2, OrTransportMode mode, OrBsdfSample *out) {
            const Cfg c = configOf((const ClosureMicrofacetBlock *)block);
            if (isDeltaLobe(c)) {
                // the delta lobe the microfacet converges to
                if (c.refract == 0) {
                    mirrorSample(c.eta, wo, out);
                    return;
                }
                if (c.refract == 1) {
                    snellSample(c.eta, wo, mode, out);
                    return;
                }
                if (!usable(wo)) {
                    failSample(out);
                    return;
                }
                const double pr = clampd(fresnel(wo[2], wo[2] < 0.0f ? 1.0 / c.eta : c.eta), 0.05, 0.95);
                const bool reflect = u2 < pr;
                const bool ok = reflect ? mirrorSample(c.eta, wo, out) : snellSample(c.eta, wo, mode, out);
                if (ok) {
                    const float p = (float)(reflect ? pr : 1.0 - pr);
                    out->pdfForward = out->pdfReverse = p; // the choice between the two delta lobes is part of this component
                }
                return;
            }
            const float dummy[3] = {0.0f, 0.0f, 1.0f};
            Space s;
            if (!makeSpace(c, wo, dummy, &s) || !(s.wo[2] > 0.0)) {
                failSample(out);
                return;
            }
            const double pr = reflectProbability(c, s);
            bool reflect = true;
            double uu2 = u2;
            if (c.refract == 1)
                reflect = false;
            else if (c.refract == 2) {
                if (u2 < pr) {
                    reflect = true;
                    uu2 = u2 / pr;
                }
                else {
                    reflect = false;
                    uu2 = (u2 - pr) / (1.0 - pr);
                }
            }
            double h[3];
            if (c.dist == Distribution::Ggx)
                sampleGgxVisibleNormal(c.alpha, s.wo, u1, uu2, h);
            else
                sampleBeckmannNormal(c.alpha, u1, uu2, h);
            const double woh = s.wo[0] * h[0] + s.wo[1] * h[1] + s.wo[2] * h[2];
            if (!(woh > 0.0)) {
                failSample(out);
                return;
            }
            double wi[3];
            if (reflect) {
                for (int i = 0; i < 3; ++i)
                    wi[i] = 2.0 * woh * h[i] - s.wo[i];
                if (!(wi[2] > 0.0)) {
                    failSample(out);
                    return;
                }
            }
            else if (!refractThrough(s.wo, h, s.etaRel, wi) || !(wi[2] < 0.0)) {
                failSample(out);
                return;
            }
            const double sign = s.flip ? -1.0 : 1.0;
            const float wiWorld[3] = {(float)wi[0], (float)wi[1], (float)(sign * wi[2])};
            OrBsdfEval e;
            microfacetEvaluate(block, wo, wiWorld, mode, &e);
            if (!(e.pdfForward > 0.0f) || !(e.f[0] > 0.0f)) {
                failSample(out);
                return;
            }
            memset(out, 0, sizeof(*out));
            copyToSample(e, out);
            memcpy(out->wi, wiWorld, sizeof(wiWorld));
            out->eta = reflect ? 1.0f : (float)s.etaRel;
        }

        float microfacetAlbedo(const void *block, const float wo[3]) {
            const Cfg c = configOf((const ClosureMicrofacetBlock *)block);
            if (!usable(wo))
                return kBsdfAlbedoFloor;
            if (c.refract == 2)
                return 1.0f;
            if (c.refract == 0) {
                if (wo[2] < 0.0f)
                    return kBsdfAlbedoFloor;
                return c.eta > 0.0 ? albedoFrom(fresnel(wo[2], c.eta)) : 1.0f;
            }
            return albedoFrom(1.0 - fresnel(wo[2], c.eta));
        }

        const OrBsdfComponentOps kDiffuseOps = {diffuseEvaluate, diffuseSample, diffusePdf, diffuseAlbedo};
        const OrBsdfComponentOps kOrenNayarOps = {orenNayarEvaluate, orenNayarSample, orenNayarPdf, diffuseAlbedo};
        const OrBsdfComponentOps kMicrofacetOps = {microfacetEvaluate, microfacetSample, microfacetPdf, microfacetAlbedo};
        const OrBsdfComponentOps kReflectionOps = {reflectionEvaluate, reflectionSample, reflectionEvaluate, reflectionAlbedo};
        const OrBsdfComponentOps kRefractionOps = {refractionEvaluate, refractionSample, refractionEvaluate, refractionAlbedo};

    } // namespace

    const OrBsdfComponentOps *bsdfComponentOps(int32_t id) {
        switch (id) {
            case kClosureDiffuse:
                return &kDiffuseOps;
            case kClosureOrenNayar:
                return &kOrenNayarOps;
            case kClosureMicrofacet:
                return &kMicrofacetOps;
            case kClosureReflection:
                return &kReflectionOps;
            case kClosureRefraction:
                return &kRefractionOps;
            default:
                return nullptr;
        }
    }

    uint32_t bsdfLobeKind(int32_t id, const void *block, bool *isDelta) {
        bool delta = false;
        uint32_t kind = 0;
        switch (id) {
            case kClosureDiffuse:
            case kClosureOrenNayar:
                kind = OR_BSDF_REFLECTION | OR_BSDF_DIFFUSE;
                break;
            case kClosureReflection:
                kind = OR_BSDF_REFLECTION | OR_BSDF_SPECULAR;
                delta = true;
                break;
            case kClosureRefraction:
                kind = OR_BSDF_TRANSMISSION | OR_BSDF_SPECULAR;
                delta = true;
                break;
            case kClosureMicrofacet: {
                const Cfg c = configOf((const ClosureMicrofacetBlock *)block);
                delta = isDeltaLobe(c);
                kind = (c.refract == 0 ? OR_BSDF_REFLECTION : (c.refract == 1 ? OR_BSDF_TRANSMISSION : (OR_BSDF_REFLECTION | OR_BSDF_TRANSMISSION))) | (delta ? OR_BSDF_SPECULAR : OR_BSDF_GLOSSY);
                break;
            }
            default:
                break;
        }
        if (isDelta)
            *isDelta = delta;
        return kind;
    }

} // namespace openrender::shading
