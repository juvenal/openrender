/**
 * Project: openRender
 *
 * File: crossingMath.h
 *
 * Description:
 *   Exact ray-crossing counts for the flat and bilinear primitives
 *   (triangles, bilinear quads and patches) behind
 *   CSurface::countCrossings() (spec 021).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	crossingMath.h
//  Classes				:	-
//  Description			:	Unsigned crossing counts of a ray from + t*dir,
//							t > 0, with a triangle or a bilinear quad, in
//							double precision. Orientation and culling are
//							ignored: these are geometric queries.
//
////////////////////////////////////////////////////////////////////////
#ifndef CROSSINGMATH_H
#define CROSSINGMATH_H

#include <cmath>

// Crossings (0 or 1) of the ray with the triangle (v0, v1, v2)
template <class T>
inline int countTriangleCrossings(const T *v0, const T *v1, const T *v2, const float *from, const float *dir) {
    double e1[3], e2[3], p[3], s[3], q[3];
    for (int k = 0; k < 3; k++) {
        e1[k] = (double)v1[k] - v0[k];
        e2[k] = (double)v2[k] - v0[k];
        s[k] = (double)from[k] - v0[k];
    }
    p[0] = dir[1] * e2[2] - dir[2] * e2[1];
    p[1] = dir[2] * e2[0] - dir[0] * e2[2];
    p[2] = dir[0] * e2[1] - dir[1] * e2[0];
    const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    const double scale = std::sqrt((e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]) * (e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2]) *
                                   ((double)dir[0] * dir[0] + (double)dir[1] * dir[1] + (double)dir[2] * dir[2]));
    if (std::fabs(det) <= 1e-12 * scale)
        return 0; // The ray is parallel to the triangle's plane
    const double inv = 1.0 / det;
    const double u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
    if (u < 0 || u > 1)
        return 0;
    q[0] = s[1] * e1[2] - s[2] * e1[1];
    q[1] = s[2] * e1[0] - s[0] * e1[2];
    q[2] = s[0] * e1[1] - s[1] * e1[0];
    const double v = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
    if (v < 0 || u + v > 1)
        return 0;
    const double t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
    return t > 0 ? 1 : 0;
}

// Crossings (0, 1 or 2) of the ray with the bilinear quad
// P(u, v) = (1-u)(1-v) P00 + u(1-v) P10 + (1-u)v P01 + uv P11, u, v in [0, 1]
template <class T>
inline int countBilinearCrossings(const T *P00, const T *P10, const T *P01, const T *P11, const float *from, const float *dir) {
    double a[3], b[3], c[3], d[3], q[3];
    for (int k = 0; k < 3; k++) {
        a[k] = (double)P11[k] - P10[k] - P01[k] + P00[k];
        b[k] = (double)P10[k] - P00[k];
        c[k] = (double)P01[k] - P00[k];
        d[k] = (double)P00[k] - from[k];
        q[k] = dir[k];
    }
    const double qq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2];
    if (qq <= 0)
        return 0;

    // Two orthonormal directions perpendicular to the ray: the hit satisfies
    // e . (P(u, v) - from) = 0 for both
    double e[2][3];
    {
        const double ql = std::sqrt(qq);
        const double n[3] = {q[0] / ql, q[1] / ql, q[2] / ql};
        double h[3] = {0, 0, 0};
        const int m = (std::fabs(n[0]) <= std::fabs(n[1]) && std::fabs(n[0]) <= std::fabs(n[2])) ? 0 : (std::fabs(n[1]) <= std::fabs(n[2]) ? 1 : 2);
        h[m] = 1;
        e[0][0] = n[1] * h[2] - n[2] * h[1];
        e[0][1] = n[2] * h[0] - n[0] * h[2];
        e[0][2] = n[0] * h[1] - n[1] * h[0];
        const double l = std::sqrt(e[0][0] * e[0][0] + e[0][1] * e[0][1] + e[0][2] * e[0][2]);
        for (int k = 0; k < 3; k++)
            e[0][k] /= l;
        e[1][0] = n[1] * e[0][2] - n[2] * e[0][1];
        e[1][1] = n[2] * e[0][0] - n[0] * e[0][2];
        e[1][2] = n[0] * e[0][1] - n[1] * e[0][0];
    }
    double A[2], B[2], C[2], D[2];
    for (int i = 0; i < 2; i++) {
        A[i] = e[i][0] * a[0] + e[i][1] * a[1] + e[i][2] * a[2];
        B[i] = e[i][0] * b[0] + e[i][1] * b[1] + e[i][2] * b[2];
        C[i] = e[i][0] * c[0] + e[i][1] * c[1] + e[i][2] * c[2];
        D[i] = e[i][0] * d[0] + e[i][1] * d[1] + e[i][2] * d[2];
    }

    // Eliminate u: (A2 C1 - A1 C2) v^2 + (A2 D1 - A1 D2 + B2 C1 - B1 C2) v + (B2 D1 - B1 D2) = 0
    const double qa = A[1] * C[0] - A[0] * C[1];
    const double qb = A[1] * D[0] - A[0] * D[1] + B[1] * C[0] - B[0] * C[1];
    const double qc = B[1] * D[0] - B[0] * D[1];
    const double size = std::fabs(qa) + std::fabs(qb) + std::fabs(qc);
    if (size == 0)
        return 0;

    double roots[2];
    int nr = 0;
    if (std::fabs(qa) <= 1e-12 * size) {
        if (std::fabs(qb) <= 1e-12 * size)
            return 0;
        roots[nr++] = -qc / qb;
    }
    else {
        const double disc = qb * qb - 4 * qa * qc;
        if (disc < 0)
            return 0;
        const double sq = std::sqrt(disc);
        const double r = -0.5 * (qb + (qb >= 0 ? sq : -sq));
        roots[nr++] = r / qa;
        if (r != 0)
            roots[nr++] = qc / r;
        if (nr == 2 && roots[0] == roots[1])
            nr = 1;
    }

    int count = 0;
    for (int i = 0; i < nr; i++) {
        const double v = roots[i];
        if (v < 0 || v > 1)
            continue;
        const double den0 = A[0] * v + B[0], den1 = A[1] * v + B[1];
        double u;
        if (std::fabs(den0) >= std::fabs(den1)) {
            if (den0 == 0)
                continue;
            u = -(C[0] * v + D[0]) / den0;
        }
        else
            u = -(C[1] * v + D[1]) / den1;
        if (u < 0 || u > 1)
            continue;
        const double t = ((a[0] * u * v + b[0] * u + c[0] * v + d[0]) * q[0] + (a[1] * u * v + b[1] * u + c[1] * v + d[1]) * q[1] +
                          (a[2] * u * v + b[2] * u + c[2] * v + d[2]) * q[2]) /
                         qq;
        if (t > 0)
            count++;
    }
    return count;
}

#endif // CROSSINGMATH_H
