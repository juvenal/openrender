/**
 * Project: openRender
 *
 * File: rslOps.cpp
 *
 * Description:
 *   Batch (instruction-outer) RSL opcode implementations.
 *   Single source of truth used by the LLVM JIT (.slo path).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "rslOps.h"
#include "activeContext.h"
#include "shading.h"

#include <cmath>
#include <cstring>

// =========================================================================
// Internal helpers
// =========================================================================

// Index into an array with stride: for uniform (str=0) always returns index 0.
#define IDX(base, str, i) ((base) + (str) * (i))

// Active-vertex guard used in tag-checked loops.
#define ACTIVE(tags, i) (!(tags) || (tags)[i] == 0)

// =========================================================================
// Arithmetic — vector
// =========================================================================

void op_addvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] + IDX(b,sb,i)[0];
        IDX(dst,sd,i)[1] = IDX(a,sa,i)[1] + IDX(b,sb,i)[1];
        IDX(dst,sd,i)[2] = IDX(a,sa,i)[2] + IDX(b,sb,i)[2];
    }
}
void op_subvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] - IDX(b,sb,i)[0];
        IDX(dst,sd,i)[1] = IDX(a,sa,i)[1] - IDX(b,sb,i)[1];
        IDX(dst,sd,i)[2] = IDX(a,sa,i)[2] - IDX(b,sb,i)[2];
    }
}
void op_mulvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] * IDX(b,sb,i)[0];
        IDX(dst,sd,i)[1] = IDX(a,sa,i)[1] * IDX(b,sb,i)[1];
        IDX(dst,sd,i)[2] = IDX(a,sa,i)[2] * IDX(b,sb,i)[2];
    }
}
void op_divvv(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] / IDX(b,sb,i)[0];
        IDX(dst,sd,i)[1] = IDX(a,sa,i)[1] / IDX(b,sb,i)[1];
        IDX(dst,sd,i)[2] = IDX(a,sa,i)[2] / IDX(b,sb,i)[2];
    }
}
void op_negv(float* dst, int sd, const float* a, int sa, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = -IDX(a,sa,i)[0];
        IDX(dst,sd,i)[1] = -IDX(a,sa,i)[1];
        IDX(dst,sd,i)[2] = -IDX(a,sa,i)[2];
    }
}
void op_addvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float fi = IDX(f,sf,i)[0];
        IDX(dst,sd,i)[0] = IDX(v,sv,i)[0] + fi;
        IDX(dst,sd,i)[1] = IDX(v,sv,i)[1] + fi;
        IDX(dst,sd,i)[2] = IDX(v,sv,i)[2] + fi;
    }
}
void op_subvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float fi = IDX(f,sf,i)[0];
        IDX(dst,sd,i)[0] = IDX(v,sv,i)[0] - fi;
        IDX(dst,sd,i)[1] = IDX(v,sv,i)[1] - fi;
        IDX(dst,sd,i)[2] = IDX(v,sv,i)[2] - fi;
    }
}
void op_mulvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float fi = IDX(f,sf,i)[0];
        IDX(dst,sd,i)[0] = IDX(v,sv,i)[0] * fi;
        IDX(dst,sd,i)[1] = IDX(v,sv,i)[1] * fi;
        IDX(dst,sd,i)[2] = IDX(v,sv,i)[2] * fi;
    }
}
void op_divvf(float* dst, int sd, const float* v, int sv, const float* f, int sf, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float fi = IDX(f,sf,i)[0];
        IDX(dst,sd,i)[0] = IDX(v,sv,i)[0] / fi;
        IDX(dst,sd,i)[1] = IDX(v,sv,i)[1] / fi;
        IDX(dst,sd,i)[2] = IDX(v,sv,i)[2] / fi;
    }
}

// =========================================================================
// Arithmetic — float
// =========================================================================

void op_addff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] + IDX(b,sb,i)[0];
}
void op_subff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] - IDX(b,sb,i)[0];
}
void op_mulff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] * IDX(b,sb,i)[0];
}
void op_divff(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] / IDX(b,sb,i)[0];
}
void op_negf(float* dst, int sd, const float* a, int sa, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = -IDX(a,sa,i)[0];
}

// =========================================================================
// Float comparisons
// =========================================================================

void op_flt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] < IDX(b,sb,i)[0] ? 1.0f : 0.0f;
}
void op_fle(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] <= IDX(b,sb,i)[0] ? 1.0f : 0.0f;
}
void op_fgt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] > IDX(b,sb,i)[0] ? 1.0f : 0.0f;
}
void op_fge(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] >= IDX(b,sb,i)[0] ? 1.0f : 0.0f;
}
void op_feq(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] == IDX(b,sb,i)[0] ? 1.0f : 0.0f;
}
void op_fne(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] != IDX(b,sb,i)[0] ? 1.0f : 0.0f;
}

// =========================================================================
// Math — scalar float
// =========================================================================

void op_sqrt       (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=sqrtf(IDX(a,sa,i)[0]); }
void op_inversesqrt(float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) { float v=IDX(a,sa,i)[0]; IDX(dst,sd,i)[0]=(v>0)?1.0f/sqrtf(v):0.0f; } }
void op_abs        (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=fabsf(IDX(a,sa,i)[0]); }
void op_sign       (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) { float v=IDX(a,sa,i)[0]; IDX(dst,sd,i)[0]=(v>0.0f)?1.0f:(v<0.0f)?-1.0f:0.0f; } }
void op_floor      (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=floorf(IDX(a,sa,i)[0]); }
void op_ceil       (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=ceilf(IDX(a,sa,i)[0]); }
void op_exp        (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=expf(IDX(a,sa,i)[0]); }
void op_log        (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) { float v=IDX(a,sa,i)[0]; IDX(dst,sd,i)[0]=(v>0.0f)?logf(v):0.0f; } }
void op_sin        (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=sinf(IDX(a,sa,i)[0]); }
void op_cos        (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=cosf(IDX(a,sa,i)[0]); }
void op_tan        (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=tanf(IDX(a,sa,i)[0]); }
void op_asin       (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=asinf(IDX(a,sa,i)[0]); }
void op_acos       (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=acosf(IDX(a,sa,i)[0]); }
void op_atan       (float* dst, int sd, const float* a, int sa, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=atanf(IDX(a,sa,i)[0]); }
void op_atan2      (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=atan2f(IDX(a,sa,i)[0],IDX(b,sb,i)[0]); }
void op_pow        (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=powf(IDX(a,sa,i)[0],IDX(b,sb,i)[0]); }
void op_mod        (float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=fmodf(IDX(a,sa,i)[0],IDX(b,sb,i)[0]); }

void op_clampf(float* dst, int sd, const float* v, int sv, const float* lo, int sl, const float* hi, int sh, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float val = IDX(v,sv,i)[0], vlo = IDX(lo,sl,i)[0], vhi = IDX(hi,sh,i)[0];
        IDX(dst,sd,i)[0] = val < vlo ? vlo : (val > vhi ? vhi : val);
    }
}
void op_mixf(float* dst, int sd, const float* a, int sa, const float* b, int sb, const float* t, int st, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float ti = IDX(t,st,i)[0];
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0]*(1.0f-ti) + IDX(b,sb,i)[0]*ti;
    }
}

// =========================================================================
// Math — vector
// =========================================================================

void op_normalize(float* dst, int sd, const float* a, int sa, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float x=IDX(a,sa,i)[0], y=IDX(a,sa,i)[1], z=IDX(a,sa,i)[2];
        float len2 = x*x + y*y + z*z;
        float inv = (len2 > 1e-16f) ? 1.0f/sqrtf(len2) : 0.0f;
        IDX(dst,sd,i)[0] = x*inv;
        IDX(dst,sd,i)[1] = y*inv;
        IDX(dst,sd,i)[2] = z*inv;
    }
}
void op_length(float* dst, int sd, const float* a, int sa, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float x=IDX(a,sa,i)[0], y=IDX(a,sa,i)[1], z=IDX(a,sa,i)[2];
        IDX(dst,sd,i)[0] = sqrtf(x*x + y*y + z*z);
    }
}
void op_dot(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0]*IDX(b,sb,i)[0]
                         + IDX(a,sa,i)[1]*IDX(b,sb,i)[1]
                         + IDX(a,sa,i)[2]*IDX(b,sb,i)[2];
}
void op_cross(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float ax=IDX(a,sa,i)[0], ay=IDX(a,sa,i)[1], az=IDX(a,sa,i)[2];
        float bx=IDX(b,sb,i)[0], by=IDX(b,sb,i)[1], bz=IDX(b,sb,i)[2];
        IDX(dst,sd,i)[0] = ay*bz - az*by;
        IDX(dst,sd,i)[1] = az*bx - ax*bz;
        IDX(dst,sd,i)[2] = ax*by - ay*bx;
    }
}
void op_clampv(float* dst, int sd, const float* v, int sv, const float* lo, int sl, const float* hi, int sh, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) for (int k=0;k<3;k++) {
        float val=IDX(v,sv,i)[k], vlo=IDX(lo,sl,i)[k], vhi=IDX(hi,sh,i)[k];
        IDX(dst,sd,i)[k] = val < vlo ? vlo : (val > vhi ? vhi : val);
    }
}
void op_mixv(float* dst, int sd, const float* a, int sa, const float* b, int sb, const float* t, int st, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float ti = IDX(t,st,i)[0];
        for (int k=0;k<3;k++) IDX(dst,sd,i)[k] = IDX(a,sa,i)[k]*(1.0f-ti) + IDX(b,sb,i)[k]*ti;
    }
}

// =========================================================================
// Component access
// =========================================================================

void op_xcomp   (float* dst, int sd, const float* v, int sv, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=IDX(v,sv,i)[0]; }
void op_ycomp   (float* dst, int sd, const float* v, int sv, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=IDX(v,sv,i)[1]; }
void op_zcomp   (float* dst, int sd, const float* v, int sv, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0]=IDX(v,sv,i)[2]; }
void op_setxcomp(float* v, int sv, const float* f, int sf, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(v,sv,i)[0]=IDX(f,sf,i)[0]; }
void op_setycomp(float* v, int sv, const float* f, int sf, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(v,sv,i)[1]=IDX(f,sf,i)[0]; }
void op_setzcomp(float* v, int sv, const float* f, int sf, int n, const int* tags) { for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(v,sv,i)[2]=IDX(f,sf,i)[0]; }

// =========================================================================
// Construction
// =========================================================================

void op_vfromf(float* dst, int sd, const float* f, int sf, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float v = IDX(f,sf,i)[0];
        IDX(dst,sd,i)[0] = IDX(dst,sd,i)[1] = IDX(dst,sd,i)[2] = v;
    }
}
void op_vfromvff(float* dst, int sd, const float* v, int sv, const float* f1, int s1, const float* f2, int s2, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = IDX(v,sv,i)[0];
        IDX(dst,sd,i)[1] = IDX(f1,s1,i)[0];
        IDX(dst,sd,i)[2] = IDX(f2,s2,i)[0];
    }
}
void op_vfromfff(float* dst, int sd, const float* f0, int s0, const float* f1, int s1, const float* f2, int s2, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = IDX(f0,s0,i)[0];
        IDX(dst,sd,i)[1] = IDX(f1,s1,i)[0];
        IDX(dst,sd,i)[2] = IDX(f2,s2,i)[0];
    }
}

// =========================================================================
// Move / copy
// =========================================================================

void op_moveff(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(src,ss,i)[0];
}
void op_movevv(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        IDX(dst,sd,i)[0] = IDX(src,ss,i)[0];
        IDX(dst,sd,i)[1] = IDX(src,ss,i)[1];
        IDX(dst,sd,i)[2] = IDX(src,ss,i)[2];
    }
}

void op_vufloat(float* dst, float val) {
    dst[0] = val;
}
void op_vuvector(float* dst, const float* src) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// =========================================================================
// smoothstep
// =========================================================================

void rsl_smoothstep(float* dst, int sd,
                    const float* e0, int s0,
                    const float* e1, int s1,
                    const float* x,  int sx,
                    int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float a = IDX(e0,s0,i)[0], b = IDX(e1,s1,i)[0], v = IDX(x,sx,i)[0];
        float t = (b - a > 1e-20f) ? (v - a) / (b - a) : 0.0f;
        if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
        IDX(dst,sd,i)[0] = t * t * (3.0f - 2.0f * t);
    }
}

// =========================================================================
// Geometry
// =========================================================================

void op_faceforward(float* dst, int sd,
                    const float* n_in, int sn,
                    const float* i_in, int si,
                    const float* ng,   int sng,
                    int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        // dot(N, Ng) and dot(I, Ng) — if (N·Ng) > 0 and (I·Ng) > 0, flip N
        float dotNNg = 0.0f, dotINg = 0.0f;
        for (int k=0;k<3;k++) {
            dotNNg += IDX(n_in,sn,i)[k] * IDX(ng,sng,i)[k];
            dotINg += IDX(i_in,si,i)[k] * IDX(ng,sng,i)[k];
        }
        float sign = 1.0f;
        if (dotNNg > 0.0f) sign = (dotINg > 0.0f) ? -1.0f :  1.0f;
        else                sign = (dotINg > 0.0f) ?  1.0f : -1.0f;
        for (int k=0;k<3;k++) IDX(dst,sd,i)[k] = IDX(n_in,sn,i)[k] * sign;
    }
}

// =========================================================================
// Conditional tag management — exact interpreter semantics (IF2EXPR)
// =========================================================================

void op_if_update(const float* cond, int scond, int* tags, int n, int* numActive, int* numPassive) {
    for (int i = 0; i < n; i++) {
        if (tags[i] > 0) {
            tags[i]++;
        } else {
            if (cond ? IDX(cond,scond,i)[0] != 0.0f : false) {
                tags[i] = 0;                   // condition true → stay active
            } else {
                tags[i] = 1;                   // condition false → inactive
                if (numActive)  (*numActive)--;
                if (numPassive) (*numPassive)++;
            }
        }
    }
}

void op_else_update(int* tags, int n, int* numActive, int* numPassive) {
    for (int i = 0; i < n; i++) {
        if (tags[i] <= 1) {
            if (tags[i] == 1) {
                tags[i] = 0;
                if (numActive)  (*numActive)++;
                if (numPassive) (*numPassive)--;
            } else {
                tags[i] = 1;
                if (numActive)  (*numActive)--;
                if (numPassive) (*numPassive)++;
            }
        }
    }
}

void op_endif_update(int* tags, int n, int* numActive, int* numPassive) {
    for (int i = 0; i < n; i++) {
        if (tags[i] > 0) {
            tags[i]--;
            if (tags[i] == 0) {
                if (numActive)  (*numActive)++;
                if (numPassive) (*numPassive)--;
            }
        }
    }
}

// =========================================================================
// Lighting — batch wrappers (same functions the interpreter uses)
// =========================================================================

void op_ambient_batch(float* result, int n [[maybe_unused]], const int* tags) {
    (void)tags; // callAmbient handles tags internally through ss->tags
    CShadingContext *ctx = libshader::activeContext();
    if (ctx) ctx->callAmbient(result);
}

void op_diffuse_batch(float* result, int sr, const float* Nf, int sn [[maybe_unused]], int n, const int* tags) {
    (void)sr; (void)n; (void)tags;
    CShadingContext *ctx = libshader::activeContext();
    if (ctx) ctx->callDiffuse(result, Nf);
}

void op_specular_batch(float* result, const float* Nf, const float* V,
                       const float* roughness, int n, const int* tags) {
    (void)n; (void)tags;
    CShadingContext *ctx = libshader::activeContext();
    if (ctx) ctx->callSpecular(result, Nf, V, roughness ? roughness[0] : 0.1f);
}

// =========================================================================
// Shader attribute query
// =========================================================================

void op_lightsource_f(float* result, int sr, const char* attrName, float* outParam, int so, int n, const int* tags) {
    (void)n; (void)tags;
    float value = 0.0f;
    CShadingContext *ctx = libshader::activeContext();
    if (ctx) {
        CShadingState *ss = ctx->currentShadingState;
        if (ss && ss->currentLightInstance) {
            CProgrammableShaderInstance *light = (CProgrammableShaderInstance *)ss->currentLightInstance;
            if (light) light->getParameter(attrName, &value, nullptr, nullptr);
        }
    }
    if (result)   IDX(result,   sr, 0)[0] = value;
    if (outParam) IDX(outParam, so, 0)[0] = value;
}

// =========================================================================
// Coordinate transforms
// =========================================================================

static bool getFromMatrix(const char* space, const float*& fromMat) {
    CShadingContext *ctx = libshader::activeContext();
    CRendererServices *svc = ctx ? ctx->getServices() : nullptr;
    if (!svc) { fromMat = nullptr; return false; }
    const float *to = nullptr;
    ECoordinateSystem dummy;
    if (svc->findCoordinateSystemWithType(space, fromMat, to, dummy) && fromMat != nullptr)
        return true;
    fromMat = nullptr;
    return false;
}
static bool getToMatrix(const char* space, const float*& toMat) {
    CShadingContext *ctx = libshader::activeContext();
    CRendererServices *svc = ctx ? ctx->getServices() : nullptr;
    if (!svc) { toMat = nullptr; return false; }
    const float *from = nullptr;
    ECoordinateSystem dummy;
    if (svc->findCoordinateSystemWithType(space, from, toMat, dummy) && toMat != nullptr)
        return true;
    toMat = nullptr;
    return false;
}

void op_pfrom(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *from = nullptr;
    if (!space || !getFromMatrix(space, from)) {
        for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
            IDX(dst,sd,i)[0]=IDX(src,ss,i)[0];
            IDX(dst,sd,i)[1]=IDX(src,ss,i)[1];
            IDX(dst,sd,i)[2]=IDX(src,ss,i)[2];
        }
        return;
    }
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *p = IDX(src,ss,i);
        float *r = IDX(dst,sd,i);
        // Homogeneous point transform by column-major 4×4 matrix
        float pw = from[3]*p[0] + from[7]*p[1] + from[11]*p[2] + from[15];
        float inv = (fabsf(pw) > 1e-20f) ? 1.0f/pw : 1.0f;
        r[0] = (from[0]*p[0] + from[4]*p[1] + from[8]*p[2]  + from[12]) * inv;
        r[1] = (from[1]*p[0] + from[5]*p[1] + from[9]*p[2]  + from[13]) * inv;
        r[2] = (from[2]*p[0] + from[6]*p[1] + from[10]*p[2] + from[14]) * inv;
    }
}

void op_vtransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *from = nullptr;
    if (!space || !getFromMatrix(space, from)) {
        for (int i=0;i<n;i++) if(ACTIVE(tags,i)) {
            IDX(dst,sd,i)[0]=IDX(src,ss,i)[0];
            IDX(dst,sd,i)[1]=IDX(src,ss,i)[1];
            IDX(dst,sd,i)[2]=IDX(src,ss,i)[2];
        }
        return;
    }
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *p = IDX(src,ss,i);
        float *r = IDX(dst,sd,i);
        r[0] = from[0]*p[0] + from[4]*p[1] + from[8]*p[2];
        r[1] = from[1]*p[0] + from[5]*p[1] + from[9]*p[2];
        r[2] = from[2]*p[0] + from[6]*p[1] + from[10]*p[2];
    }
}

void op_ntransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *toMat = nullptr;
    if (!space || !getToMatrix(space, toMat)) {
        for (int i=0;i<n;i++) if(ACTIVE(tags,i)) {
            IDX(dst,sd,i)[0]=IDX(src,ss,i)[0];
            IDX(dst,sd,i)[1]=IDX(src,ss,i)[1];
            IDX(dst,sd,i)[2]=IDX(src,ss,i)[2];
        }
        return;
    }
    // Normals transform by transposed inverse; use to-matrix (transpose columns 0-2)
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *p = IDX(src,ss,i);
        float *r = IDX(dst,sd,i);
        r[0] = toMat[0]*p[0] + toMat[1]*p[1] + toMat[2]*p[2];
        r[1] = toMat[4]*p[0] + toMat[5]*p[1] + toMat[6]*p[2];
        r[2] = toMat[8]*p[0] + toMat[9]*p[1] + toMat[10]*p[2];
    }
}
