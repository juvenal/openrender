/**
 * Project: openRender
 *
 * File: bsdfMath.cpp
 *
 * Description:
 *   The shared BRDF math library (spec 035, bsdfMath.h). Written from the papers listed in specs/035-closure-tree-bsdf/bsdf-sources.md.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "bsdfMath.h"

#include <math.h>
#include <string.h>

namespace openrender::shading::bsdf {

    namespace {
        constexpr double kPi = 3.14159265358979323846;

    } // namespace

    Distribution distributionFromName(const char *name) { return (name != nullptr && strcmp(name, "beckmann") == 0) ? Distribution::Beckmann : Distribution::Ggx; }

    // ---- D ----

    double microfacetD(Distribution d, double alpha, double c) {
        if (!(c > 0.0))
            return 0.0;
        const double a2 = alpha * alpha;
        if (d == Distribution::Ggx) {
            // D = alpha^2 / (pi cos^4 (alpha^2 + tan^2)^2) = alpha^2 / (pi (cos^2 (alpha^2 - 1) + 1)^2)
            const double den = c * c * (a2 - 1.0) + 1.0;
            return a2 / (kPi * den * den);
        }
        // Beckmann: exp(-tan^2 / alpha^2) / (pi alpha^2 cos^4)
        const double c2 = c * c;
        const double tan2 = (1.0 - c2) / c2;
        return exp(-tan2 / a2) / (kPi * a2 * c2 * c2);
    }

    float ggxD(float alpha, float cosThetaM) { return (float)microfacetD(Distribution::Ggx, alpha, cosThetaM); }
    float beckmannD(float alpha, float cosThetaM) { return (float)microfacetD(Distribution::Beckmann, alpha, cosThetaM); }

    // ---- Smith Lambda (Heitz 2014; the Beckmann form is the rational approximation of Walter et al. 2007) ----

    double smithLambda(Distribution d, double alpha, double cosTheta) {
        const double c = fabs(cosTheta);
        if (!(c > 0.0))
            return 1e30; // grazing: fully masked
        const double s2 = fmax(0.0, 1.0 - c * c);
        const double tan2 = s2 / (c * c);
        if (d == Distribution::Ggx)
            return 0.5 * (sqrt(1.0 + alpha * alpha * tan2) - 1.0);
        const double a = 1.0 / (alpha * sqrt(tan2));
        if (!(a < 1.6))
            return 0.0;
        return (1.0 - 1.259 * a + 0.396 * a * a) / (3.535 * a + 2.181 * a * a);
    }

    // ---- Fresnel ----

    double fresnel(double cosThetaI, double eta) {
        if (!(eta > 0.0))
            return 1.0;
        double c = fabs(cosThetaI);
        if (c > 1.0)
            c = 1.0;
        if (cosThetaI < 0.0)
            eta = 1.0 / eta;
        const double sin2t = (1.0 - c * c) / (eta * eta);
        if (sin2t >= 1.0)
            return 1.0; // total internal reflection
        const double cosT = sqrt(1.0 - sin2t);
        const double rs = (c - eta * cosT) / (c + eta * cosT);
        const double rp = (eta * c - cosT) / (eta * c + cosT);
        return 0.5 * (rs * rs + rp * rp);
    }

    float fresnelDielectric(float cosThetaI, float eta) { return (float)fresnel(cosThetaI, eta); }

    // ---- frames ----

    void orthonormalBasis(const double n[3], double t[3], double b[3]) {
        const double sign = copysign(1.0, n[2]);
        const double a = -1.0 / (sign + n[2]);
        const double bb = n[0] * n[1] * a;
        t[0] = 1.0 + sign * n[0] * n[0] * a;
        t[1] = sign * bb;
        t[2] = -sign * n[0];
        b[0] = bb;
        b[1] = sign + n[1] * n[1] * a;
        b[2] = -n[1];
    }

    // ---- visible normals ----

    void sampleGgxVisibleNormal(double alpha, const double wo[3], double u1, double u2, double h[3]) {
        // Heitz 2018, "Sampling the GGX Distribution of Visible Normals": stretch, sample a disk, warp, unstretch
        double vh[3] = {alpha * wo[0], alpha * wo[1], wo[2]};
        const double vl = sqrt(vh[0] * vh[0] + vh[1] * vh[1] + vh[2] * vh[2]);
        vh[0] /= vl;
        vh[1] /= vl;
        vh[2] /= vl;
        const double lensq = vh[0] * vh[0] + vh[1] * vh[1];
        double t1[3] = {1.0, 0.0, 0.0};
        if (lensq > 1e-14) {
            const double inv = 1.0 / sqrt(lensq);
            t1[0] = -vh[1] * inv;
            t1[1] = vh[0] * inv;
            t1[2] = 0.0;
        }
        const double t2[3] = {vh[1] * t1[2] - vh[2] * t1[1], vh[2] * t1[0] - vh[0] * t1[2], vh[0] * t1[1] - vh[1] * t1[0]};
        const double r = sqrt(u1), phi = 2.0 * kPi * u2;
        const double p1 = r * cos(phi);
        double p2 = r * sin(phi);
        const double s = 0.5 * (1.0 + vh[2]);
        p2 = (1.0 - s) * sqrt(fmax(0.0, 1.0 - p1 * p1)) + s * p2;
        const double pz = sqrt(fmax(0.0, 1.0 - p1 * p1 - p2 * p2));
        const double nh[3] = {p1 * t1[0] + p2 * t2[0] + pz * vh[0], p1 * t1[1] + p2 * t2[1] + pz * vh[1], p1 * t1[2] + p2 * t2[2] + pz * vh[2]};
        h[0] = alpha * nh[0];
        h[1] = alpha * nh[1];
        h[2] = fmax(1e-12, nh[2]);
        const double l = sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
        h[0] /= l;
        h[1] /= l;
        h[2] /= l;
    }

    void sampleBeckmannNormal(double alpha, double u1, double u2, double h[3]) {
        // density D(h) cos(theta_h): tan^2 = -alpha^2 ln(1 - u1)
        const double tan2 = -alpha * alpha * log(1.0 - u1);
        const double cosT = 1.0 / sqrt(1.0 + tan2), sinT = sqrt(fmax(0.0, 1.0 - cosT * cosT)), phi = 2.0 * kPi * u2;
        h[0] = sinT * cos(phi);
        h[1] = sinT * sin(phi);
        h[2] = cosT;
    }

    double visibleNormalPdf(Distribution d, double alpha, double cosThetaO, double woDotH, double cosThetaH) {
        if (d == Distribution::Ggx) {
            if (!(woDotH > 0.0) || !(cosThetaO > 0.0))
                return 0.0;
            const double g1 = 1.0 / (1.0 + smithLambda(d, alpha, cosThetaO));
            return g1 * woDotH * microfacetD(d, alpha, cosThetaH) / cosThetaO;
        }
        return microfacetD(d, alpha, cosThetaH) * cosThetaH; // the fallback: D cos
    }

    // ---- refraction ----

    bool refractThrough(const double wo[3], const double h[3], double etaRel, double wi[3]) {
        const double c = wo[0] * h[0] + wo[1] * h[1] + wo[2] * h[2];
        const double sin2t = (1.0 - c * c) / (etaRel * etaRel);
        if (!(c > 0.0) || sin2t >= 1.0)
            return false;
        const double cosT = sqrt(1.0 - sin2t);
        const double k = c / etaRel - cosT;
        for (int i = 0; i < 3; ++i)
            wi[i] = -wo[i] / etaRel + k * h[i];
        const double l = sqrt(wi[0] * wi[0] + wi[1] * wi[1] + wi[2] * wi[2]);
        for (int i = 0; i < 3; ++i)
            wi[i] /= l;
        return true;
    }

    // ---- diffuse family ----

    void sampleCosineHemisphere(double u1, double u2, double w[3]) {
        const double r = sqrt(u1), phi = 2.0 * kPi * u2;
        w[0] = r * cos(phi);
        w[1] = r * sin(phi);
        w[2] = sqrt(fmax(0.0, 1.0 - u1));
    }

    double orenNayarFactor(double sigma, const double wo[3], const double wi[3]) {
        const double s2 = sigma * sigma;
        const double A = 1.0 - s2 / (2.0 * (s2 + 0.33));
        const double B = 0.45 * s2 / (s2 + 0.09);
        if (B == 0.0)
            return A;
        const double sinO = sqrt(fmax(0.0, 1.0 - wo[2] * wo[2])), sinI = sqrt(fmax(0.0, 1.0 - wi[2] * wi[2]));
        double cosPhi = 0.0;
        if (sinO > 1e-7 && sinI > 1e-7)
            cosPhi = (wo[0] * wi[0] + wo[1] * wi[1]) / (sinO * sinI);
        if (cosPhi < 0.0)
            cosPhi = 0.0;
        // alpha = max(theta_i, theta_o) and beta = min(theta_i, theta_o): the larger angle has the larger sine and the smaller cosine
        const double sinA = fmax(sinO, sinI), sinB = fmin(sinO, sinI), cosB = fmax(wo[2], wi[2]);
        const double tanB = cosB > 0.0 ? sinB / cosB : 0.0;
        return A + B * cosPhi * sinA * tanB;
    }

    // ---- MIS ----

    double powerHeuristic(double nf, double pf, double ng, double pg) {
        const double f = nf * pf, g = ng * pg;
        const double den = f * f + g * g;
        return den > 0.0 ? f * f / den : 0.0;
    }

    double balanceHeuristic(double nf, double pf, double ng, double pg) {
        const double f = nf * pf, g = ng * pg;
        const double den = f + g;
        return den > 0.0 ? f / den : 0.0;
    }

} // namespace openrender::shading::bsdf
