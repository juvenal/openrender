/**
 * Project: openRender
 *
 * File: areaSampling.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry): the area tables of areaSampling.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "areaSampling.h"

#include <algorithm>
#include <cmath>

namespace {

    inline void sub3(double *r, const double *a, const double *b) {
        r[0] = a[0] - b[0];
        r[1] = a[1] - b[1];
        r[2] = a[2] - b[2];
    }

    inline void cross3(double *r, const double *a, const double *b) {
        r[0] = a[1] * b[2] - a[2] * b[1];
        r[1] = a[2] * b[0] - a[0] * b[2];
        r[2] = a[0] * b[1] - a[1] * b[0];
    }

    inline double dot3(const double *a, const double *b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    inline double length3(const double *a) {
        return std::sqrt(dot3(a, a));
    }

    // Fill a face from three corners: area, and the unit normal along (p1 - p0) x (p2 - p0), flipped when `flip`
    void setFace(CAreaFace &f, const double *p0, const double *p1, const double *p2, bool flip) {
        for (int k = 0; k < 3; k++) {
            f.P[0][k] = p0[k];
            f.P[1][k] = p1[k];
            f.P[2][k] = p2[k];
        }
        double e1[3], e2[3], n[3];
        sub3(e1, p1, p0);
        sub3(e2, p2, p0);
        cross3(n, e1, e2);
        const double len = length3(n);
        f.area = 0.5 * len;
        if (len > 0.0) {
            const double s = flip ? -1.0 / len : 1.0 / len;
            for (int k = 0; k < 3; k++)
                f.normal[k] = n[k] * s;
        }
        else
            f.normal[0] = f.normal[1] = f.normal[2] = 0.0;
    }

    inline void toDouble(double *d, const float *f) {
        d[0] = f[0];
        d[1] = f[1];
        d[2] = f[2];
    }

    // The (u, v) of the point P of the planar bilinear quad A (0,0), B (1,0), D (0,1), C (1,1):
    //   P = A + u e + v f + u v g,  e = B - A, f = D - A, g = A - B - D + C
    // Projected on the plane (the two axes other than the dominant axis of the normal n) it is a quadratic in v.
    void inverseBilinear(const double quad[4][3], const double *P, const double *n, double &u, double &v) {
        const double *A = quad[0], *B = quad[1], *D = quad[2], *C = quad[3];
        double e[3], f[3], g[3], q[3];
        sub3(e, B, A);
        sub3(f, D, A);
        for (int k = 0; k < 3; k++)
            g[k] = A[k] - B[k] - D[k] + C[k];
        sub3(q, P, A);
        int axis = 0;
        if (std::fabs(n[1]) > std::fabs(n[axis]))
            axis = 1;
        if (std::fabs(n[2]) > std::fabs(n[axis]))
            axis = 2;
        const int i = (axis + 1) % 3, j = (axis + 2) % 3;
        auto cross2 = [&](const double *a, const double *b) { return a[i] * b[j] - a[j] * b[i]; };
        const double qa = -cross2(f, g);
        const double qb = cross2(q, g) - cross2(f, e);
        const double qc = cross2(q, e);
        double roots[2];
        int num = 0;
        if (std::fabs(qa) < 1e-14 * (std::fabs(qb) + 1e-300) || qa == 0.0) {
            if (qb != 0.0)
                roots[num++] = -qc / qb;
        }
        else {
            double disc = qb * qb - 4.0 * qa * qc;
            if (disc < 0.0)
                disc = 0.0;
            const double sq = std::sqrt(disc);
            const double t = -0.5 * (qb + (qb >= 0.0 ? sq : -sq));
            roots[num++] = t / qa;
            if (t != 0.0)
                roots[num++] = qc / t;
        }
        // the root nearest the unit interval
        v = 0.5;
        double best = 1e300;
        for (int r = 0; r < num; r++) {
            const double d = roots[r] < 0.0 ? -roots[r] : (roots[r] > 1.0 ? roots[r] - 1.0 : 0.0);
            if (d < best) {
                best = d;
                v = roots[r];
            }
        }
        v = std::min(1.0, std::max(0.0, v));
        double dir[3];
        for (int k = 0; k < 3; k++)
            dir[k] = e[k] + v * g[k];
        const int c = std::fabs(dir[i]) >= std::fabs(dir[j]) ? i : j;
        u = std::fabs(dir[c]) > 0.0 ? (q[c] - v * f[c]) / dir[c] : 0.0;
        u = std::min(1.0, std::max(0.0, u));
    }

} // namespace

size_t areaPickBin(const std::vector<double> &cdf, double u, double &rescaled) {
    size_t i = static_cast<size_t>(std::upper_bound(cdf.begin(), cdf.end(), u) - cdf.begin());
    if (i >= cdf.size()) {
        // u at or beyond 1: the last bin of non-zero width
        i = cdf.size() - 1;
        while (i > 0 && cdf[i] == cdf[i - 1])
            --i;
    }
    const double lo = i > 0 ? cdf[i - 1] : 0.0;
    const double hi = cdf[i];
    rescaled = hi > lo ? (u - lo) / (hi - lo) : 0.0;
    if (rescaled < 0.0)
        rescaled = 0.0;
    if (rescaled > 0.99999999999999989)
        rescaled = 0.99999999999999989;
    return i;
}

void CAreaTable::finalize() {
    area = 0.0;
    for (const CAreaFace &f : faces)
        area += f.area;
    cdf.clear();
    if (!(area > 0.0))
        return;
    cdf.reserve(faces.size());
    double sum = 0.0;
    for (const CAreaFace &f : faces) {
        sum += f.area;
        cdf.push_back(sum / area);
    }
    cdf.back() = 1.0;
}

bool CAreaTable::draw(double u1, double u2, CAreaSample &out) const {
    if (!(area > 0.0) || cdf.empty())
        return false;
    double r1;
    const CAreaFace &f = faces[areaPickBin(cdf, u1, r1)];

    // a point in the triangle, uniform by area: (r1, u2) by the square-root mapping
    const double r = std::sqrt(r1);
    const double w0 = 1.0 - r, w1 = r * (1.0 - u2), w2 = r * u2;
    double P[3];
    for (int k = 0; k < 3; k++)
        P[k] = w0 * f.P[0][k] + w1 * f.P[1][k] + w2 * f.P[2][k];

    double u, v;
    switch (map) {
        case EAreaMap::PolygonTriangle:
            u = w1 + w2;
            v = u > 1e-12 ? w1 / u : 0.5;
            break;
        case EAreaMap::BilinearQuad:
            inverseBilinear(quad, P, f.normal, u, v);
            break;
        default:
            u = w0 * f.uv[0][0] + w1 * f.uv[1][0] + w2 * f.uv[2][0];
            v = w0 * f.uv[0][1] + w1 * f.uv[1][1] + w2 * f.uv[2][1];
            break;
    }
    out.u = static_cast<float>(u);
    out.v = static_cast<float>(v);
    for (int k = 0; k < 3; k++) {
        out.P[k] = static_cast<float>(P[k]);
        out.Ng[k] = static_cast<float>(f.normal[k]);
    }
    if (surface != nullptr) {
        // the point on the true surface at the drawn (u, v)
        float Ps[3], Ns[3];
        surface->samplePoint(out.u, out.v, Ps, Ns);
        const double len = std::sqrt((double)Ns[0] * Ns[0] + (double)Ns[1] * Ns[1] + (double)Ns[2] * Ns[2]);
        for (int k = 0; k < 3; k++) {
            out.P[k] = Ps[k];
            if (len > 0.0)
                out.Ng[k] = static_cast<float>(Ns[k] / len);
        }
    }
    return true;
}

CAreaTable makeTriangleTable(const float *v0, const float *v1, const float *v2) {
    CAreaTable t;
    t.map = EAreaMap::PolygonTriangle;
    double a[3], b[3], c[3];
    toDouble(a, v0);
    toDouble(b, v1);
    toDouble(c, v2);
    // dPdu x dPdv of the triangle's parametrization is (v2 - v0) x (v1 - v0): the normal of (a, c, b)
    t.faces.resize(1);
    setFace(t.faces[0], a, b, c, true);
    t.finalize();
    return t;
}

CAreaTable makeQuadTable(const CSurface *surface, const float *v0, const float *v1, const float *v2, const float *v3) {
    double A[3], B[3], D[3], C[3];
    toDouble(A, v0);
    toDouble(B, v1);
    toDouble(D, v2);
    toDouble(C, v3);

    // the quad's normal from its diagonals, its size from its edges
    double ac[3], bd[3], n[3];
    sub3(ac, C, A);
    sub3(bd, D, B);
    cross3(n, ac, bd);
    const double nlen = length3(n);
    double size = 0.0;
    for (const double *p : {B, C, D}) {
        double d[3];
        sub3(d, p, A);
        size = std::max(size, length3(d));
    }

    CAreaTable t;
    t.map = EAreaMap::BilinearQuad;
    if (!(nlen > 1e-12 * size * size)) {
        // collapsed (no area): an empty table, never chosen
        t.faces.resize(2);
        t.finalize();
        return t;
    }

    // planar? every corner within 1e-5 of the plane through A with normal n
    bool planar = true;
    for (const double *p : {B, C, D}) {
        double d[3];
        sub3(d, p, A);
        if (std::fabs(dot3(d, n)) / nlen > 1e-5 * size)
            planar = false;
    }

    // the diagonal split: both triangles on the quad's side. Cyclic order A, B, C, D, u along A->B, v along A->D
    // (dPdu x dPdv = (B - A) x (D - A)).
    double nq[3], e1[3], e2[3];
    sub3(e1, B, A);
    sub3(e2, D, A);
    cross3(nq, e1, e2);
    if (dot3(nq, n) < 0.0) {
        for (int k = 0; k < 3; k++)
            n[k] = -n[k];
    }
    auto side = [&](const double *p, const double *q, const double *r) {
        double a[3], b[3], c[3];
        sub3(a, q, p);
        sub3(b, r, p);
        cross3(c, a, b);
        return dot3(c, n);
    };
    int diagonal = 0; // 1: A-C (ABC, ACD), 2: B-D (ABD, BCD)
    if (planar) {
        if (side(A, B, C) >= 0.0 && side(A, C, D) >= 0.0)
            diagonal = 1;
        else if (side(A, B, D) >= 0.0 && side(B, C, D) >= 0.0)
            diagonal = 2;
    }
    if (diagonal == 0)
        return makeTessellatedTable(surface); // not planar, or self-intersecting

    for (int k = 0; k < 4; k++) {
        const double *p = k == 0 ? A : k == 1 ? B : k == 2 ? D : C;
        for (int c = 0; c < 3; c++)
            t.quad[k][c] = p[c];
    }
    t.surface = nullptr;
    t.faces.resize(2);
    // both triangles lie on the quad's side, so their normals follow (B - A) x (D - A) = dPdu x dPdv
    if (diagonal == 1) {
        setFace(t.faces[0], A, B, C, false);
        setFace(t.faces[1], A, C, D, false);
    }
    else {
        setFace(t.faces[0], A, B, D, false);
        setFace(t.faces[1], B, C, D, false);
    }
    t.finalize();
    return t;
}

CAreaTable makeTessellatedTable(const CSurface *surface, int K) {
    CAreaTable t;
    t.map = EAreaMap::Affine;
    t.surface = surface;
    const int count = (K + 1) * (K + 1);
    std::vector<float> u(count), v(count), time(count, 0.0f), P(3 * count), du(3 * count), dv(3 * count), ng(3 * count), dt(3 * count), pw(4 * count);
    for (int j = 0; j <= K; j++)
        for (int i = 0; i <= K; i++) {
            u[j * (K + 1) + i] = static_cast<float>(i) / K;
            v[j * (K + 1) + i] = static_cast<float>(j) / K;
        }
    float *varying[VARIABLE_CONSTANTWIDTH + 1];
    for (unsigned int k = 0; k <= VARIABLE_CONSTANTWIDTH; k++)
        varying[k] = NULL;
    varying[VARIABLE_U] = u.data();
    varying[VARIABLE_V] = v.data();
    varying[VARIABLE_TIME] = time.data();
    varying[VARIABLE_P] = P.data();
    varying[VARIABLE_DPDU] = du.data();
    varying[VARIABLE_DPDV] = dv.data();
    varying[VARIABLE_NG] = ng.data();
    varying[VARIABLE_DPDTIME] = dt.data();
    varying[VARIABLE_PW] = pw.data();
    unsigned int up = PARAMETER_P | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_NG | PARAMETER_BEGIN_SAMPLE;
    surface->sample(0, count, varying, NULL, up);

    auto corner = [&](int i, int j, double *p) { toDouble(p, &P[3 * (j * (K + 1) + i)]); };
    t.faces.reserve(2 * K * K);
    for (int j = 0; j < K; j++)
        for (int i = 0; i < K; i++) {
            double p00[3], p10[3], p01[3], p11[3];
            corner(i, j, p00);
            corner(i + 1, j, p10);
            corner(i, j + 1, p01);
            corner(i + 1, j + 1, p11);
            const float uv00[2] = {static_cast<float>(i) / K, static_cast<float>(j) / K};
            const float uv10[2] = {static_cast<float>(i + 1) / K, static_cast<float>(j) / K};
            const float uv01[2] = {static_cast<float>(i) / K, static_cast<float>(j + 1) / K};
            const float uv11[2] = {static_cast<float>(i + 1) / K, static_cast<float>(j + 1) / K};
            // two triangles with a positive (u, v) determinant: their normals follow dPdu x dPdv
            CAreaFace a, b;
            setFace(a, p00, p10, p11, false);
            setFace(b, p00, p11, p01, false);
            for (int k = 0; k < 2; k++) {
                a.uv[0][k] = uv00[k];
                a.uv[1][k] = uv10[k];
                a.uv[2][k] = uv11[k];
                b.uv[0][k] = uv00[k];
                b.uv[1][k] = uv11[k];
                b.uv[2][k] = uv01[k];
            }
            t.faces.push_back(a);
            t.faces.push_back(b);
        }
    t.finalize();
    return t;
}
