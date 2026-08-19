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
#include "noise.h"
#include "common/colorSpace.h"
#include "includes/logging.hpp"

#include <cmath>
#include <cstring>
#include <unistd.h>

// Per-JIT-invocation grid counter — incremented at op_forend (once per grid).
// Reset never (monotonically increases across grids in a frame).
// Used to correlate op_spline_c log entries to the same grid.
static thread_local int g_jit_grid  = -1;

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
// Vector comparisons
// =========================================================================

void op_veql(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        IDX(dst,sd,i)[0] = (ai[0] == bi[0] && ai[1] == bi[1] && ai[2] == bi[2]) ? 1.0f : 0.0f;
    }
}
void op_vneql(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        IDX(dst,sd,i)[0] = (ai[0] != bi[0] || ai[1] != bi[1] || ai[2] != bi[2]) ? 1.0f : 0.0f;
    }
}
void op_velt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        IDX(dst,sd,i)[0] = (ai[0] <= bi[0] && ai[1] <= bi[1] && ai[2] <= bi[2]) ? 1.0f : 0.0f;
    }
}
void op_vlt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        IDX(dst,sd,i)[0] = (ai[0] < bi[0] && ai[1] < bi[1] && ai[2] < bi[2]) ? 1.0f : 0.0f;
    }
}
void op_vegt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        IDX(dst,sd,i)[0] = (ai[0] >= bi[0] && ai[1] >= bi[1] && ai[2] >= bi[2]) ? 1.0f : 0.0f;
    }
}
void op_vgt(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        IDX(dst,sd,i)[0] = (ai[0] > bi[0] && ai[1] > bi[1] && ai[2] > bi[2]) ? 1.0f : 0.0f;
    }
}

// =========================================================================
// Logical negation
// =========================================================================

// Mirrors the interpreter's NOTEXPR exactly: *res = (float)~((int)(*op));
void op_not(float* dst, int sd, const float* a, int sa, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = (float)(~((int)IDX(a,sa,i)[0]));
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

// Mirrors Movess (SUNARYEXPR): same shape as op_moveff — the interpreter's
// own operand declaration for Movess is float*/const float*, not char**.
void op_movess(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(src,ss,i)[0];
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
    if (n > 0)
        log_debug("[JIT-endif] grid={} tags[0]={} numActive={}", g_jit_grid,
                  tags ? tags[0] : -999, numActive ? *numActive : -999);
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

// Thread-local xform supplied by CRendererContext::init() before jitInitEntry
// so that op_pfrom/op_ptransform can resolve "shader" space at bind time,
// before any render thread sets libshader::activeContext().
static thread_local const float* s_jit_init_from = nullptr;
static thread_local const float* s_jit_init_to   = nullptr;

void jitSetInitXform(const float* fromMat, const float* toMat) {
    s_jit_init_from = fromMat;
    s_jit_init_to   = toMat;
}

static bool getFromMatrix(const char* space, const float*& fromMat) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) {
        if (s_jit_init_from && space && strcmp(space, "shader") == 0) {
            fromMat = s_jit_init_from;
            return true;
        }
        fromMat = nullptr;
        return false;
    }
    const float *to = nullptr;
    ECoordinateSystem dummy;
    ctx->jitFindCoordinateSystem(space, fromMat, to, dummy);
    return fromMat != nullptr;
}
static bool getToMatrix(const char* space, const float*& toMat) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) {
        if (s_jit_init_to && space && strcmp(space, "shader") == 0) {
            toMat = s_jit_init_to;
            return true;
        }
        toMat = nullptr;
        return false;
    }
    const float *from = nullptr;
    ECoordinateSystem dummy;
    ctx->jitFindCoordinateSystem(space, from, toMat, dummy);
    return toMat != nullptr;
}

void op_pfrom(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *from = nullptr;
    if (!space || !getFromMatrix(space, from)) {
        if (n > 0 && ACTIVE(tags, 0)) {
            const float *p0 = IDX(src,ss,0);
            log_debug("[JIT-pfrom] space='{}' identity in[0]=({:.4f},{:.4f},{:.4f})", space ? space : "null", p0[0], p0[1], p0[2]);
        }
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
        if (i == 0) log_debug("[JIT-pfrom] space='{}' in[0]=({:.4f},{:.4f},{:.4f})", space, p[0], p[1], p[2]);
        // Homogeneous point transform by column-major 4×4 matrix
        float pw = from[3]*p[0] + from[7]*p[1] + from[11]*p[2] + from[15];
        float inv = (fabsf(pw) > 1e-20f) ? 1.0f/pw : 1.0f;
        r[0] = (from[0]*p[0] + from[4]*p[1] + from[8]*p[2]  + from[12]) * inv;
        r[1] = (from[1]*p[0] + from[5]*p[1] + from[9]*p[2]  + from[13]) * inv;
        r[2] = (from[2]*p[0] + from[6]*p[1] + from[10]*p[2] + from[14]) * inv;
        if (i == 0) log_debug("[JIT-pfrom] space='{}' out[0]=({:.4f},{:.4f},{:.4f})", space, r[0], r[1], r[2]);
    }
}

// op_ptransform: RSL transform("space", P) — current→named (uses "to" matrix).
void op_ptransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *to = nullptr;
    if (!space || !getToMatrix(space, to)) {
        if (n > 0 && ACTIVE(tags, 0)) {
            const float *p0 = IDX(src,ss,0);
            log_debug("[JIT-ptransform] space='{}' identity in[0]=({:.4f},{:.4f},{:.4f})", space ? space : "null", p0[0], p0[1], p0[2]);
        }
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
        if (i == 0) log_debug("[JIT-ptransform] space='{}' in[0]=({:.4f},{:.4f},{:.4f})", space, p[0], p[1], p[2]);
        float pw = to[3]*p[0] + to[7]*p[1] + to[11]*p[2] + to[15];
        float inv = (fabsf(pw) > 1e-20f) ? 1.0f/pw : 1.0f;
        r[0] = (to[0]*p[0] + to[4]*p[1] + to[8]*p[2]  + to[12]) * inv;
        r[1] = (to[1]*p[0] + to[5]*p[1] + to[9]*p[2]  + to[13]) * inv;
        r[2] = (to[2]*p[0] + to[6]*p[1] + to[10]*p[2] + to[14]) * inv;
        if (i == 0) log_debug("[JIT-ptransform] space='{}' out[0]=({:.4f},{:.4f},{:.4f})", space, r[0], r[1], r[2]);
    }
}

// op_vtransform: RSL vtransform("space", V) — current→named (uses "to" matrix, no translation).
void op_vtransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *to = nullptr;
    if (!space || !getToMatrix(space, to)) {
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
        r[0] = to[0]*p[0] + to[4]*p[1] + to[8]*p[2];
        r[1] = to[1]*p[0] + to[5]*p[1] + to[9]*p[2];
        r[2] = to[2]*p[0] + to[6]*p[1] + to[10]*p[2];
    }
}

// op_ntransform: RSL ntransform("space", N) — normal transform = from^T * N (mulmn semantics).
void op_ntransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *from = nullptr;
    if (!space || !getFromMatrix(space, from)) {
        for (int i=0;i<n;i++) if(ACTIVE(tags,i)) {
            IDX(dst,sd,i)[0]=IDX(src,ss,i)[0];
            IDX(dst,sd,i)[1]=IDX(src,ss,i)[1];
            IDX(dst,sd,i)[2]=IDX(src,ss,i)[2];
        }
        return;
    }
    // Normals transform as from^T (= mulmn(res, from, p) in mathSpec.h)
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *p = IDX(src,ss,i);
        float *r = IDX(dst,sd,i);
        r[0] = from[0]*p[0] + from[1]*p[1] + from[2]*p[2];
        r[1] = from[4]*p[0] + from[5]*p[1] + from[6]*p[2];
        r[2] = from[8]*p[0] + from[9]*p[1] + from[10]*p[2];
    }
}

// Resolves the ECoordinateSystem for a named color space (hsv/hsl/xyz/...),
// same lookup findCoordinateSystem() uses for point spaces — colorspace names
// and point-space names share one name trie and one ECoordinateSystem enum.
// Falls back to COLOR_RGB (identity in convertColorFrom/To) when unresolved.
static void getColorCoordinateSystem(const char* space, ECoordinateSystem& cSystem) {
    cSystem = COLOR_RGB;
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx || !space) return;
    const float *from = nullptr, *to = nullptr;
    ctx->jitFindCoordinateSystem(space, from, to, cSystem);
}

// op_cfrom: RSL color "space" (...) constructor — mirrors CFROMEXPR, delegates
// to the interpreter's own convertColorFrom() (src/common/colorSpace.cpp).
void op_cfrom(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    ECoordinateSystem cSystem;
    getColorCoordinateSystem(space, cSystem);
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        convertColorFrom(IDX(dst,sd,i), IDX(src,ss,i), cSystem);
    }
}

// op_ctransform: RSL ctransform("space", c) — mirrors CTRANSFORMEXPR, delegates
// to convertColorTo(). Distinct math from op_pfrom's point-matrix transform.
void op_ctransform(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    ECoordinateSystem cSystem;
    getColorCoordinateSystem(space, cSystem);
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        convertColorTo(IDX(dst,sd,i), IDX(src,ss,i), cSystem);
    }
}

// op_mfrom: RSL matrix "space" (...) constructor — mirrors MFROMEXPR (mulmm(res, from, op)).
void op_mfrom(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags) {
    const float *from = nullptr;
    if (!space || !getFromMatrix(space, from)) {
        for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
            memcpy(IDX(dst,sd,i), IDX(src,ss,i), 16 * sizeof(float));
        }
        return;
    }
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        mulmm(IDX(dst,sd,i), from, IDX(src,ss,i));
    }
}

// =========================================================================
// Matrix arithmetic
// =========================================================================

void op_addmm(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        float *r = IDX(dst,sd,i);
        for (int k = 0; k < 16; k++) r[k] = ai[k] + bi[k];
    }
}

void op_submm(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i), *bi = IDX(b,sb,i);
        float *r = IDX(dst,sd,i);
        for (int k = 0; k < 16; k++) r[k] = ai[k] - bi[k];
    }
}

// Mirrors MULMMEXPR exactly, including the temp: guards against dst aliasing an operand.
void op_mulmm(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        matrix mtmp;
        mulmm(mtmp, IDX(a,sa,i), IDX(b,sb,i));
        movmm(IDX(dst,sd,i), mtmp);
    }
}

// Mirrors DIVMMEXPR exactly: op1 * invert(op2).
void op_divmm(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        matrix inv;
        invertm(inv, IDX(b,sb,i));
        mulmm(IDX(dst,sd,i), IDX(a,sa,i), inv);
    }
}

void op_negm(float* dst, int sd, const float* a, int sa, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *ai = IDX(a,sa,i);
        float *r = IDX(dst,sd,i);
        for (int k = 0; k < 16; k++) r[k] = -ai[k];
    }
}

void op_movemm(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        memcpy(IDX(dst,sd,i), IDX(src,ss,i), 16 * sizeof(float));
    }
}

void op_mfromv(float* dst, int sd, const float* v, int sv, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        const float *vi = IDX(v,sv,i);
        float *r = IDX(dst,sd,i);
        r[0]=vi[0]; r[1]=0; r[2]=0; r[3]=0;
        r[4]=0; r[5]=vi[1]; r[6]=0; r[7]=0;
        r[8]=0; r[9]=0; r[10]=vi[2]; r[11]=0;
        r[12]=0; r[13]=0; r[14]=0; r[15]=1;
    }
}

void op_mfromf(float* dst, int sd, const float* f, int sf, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float fi = IDX(f,sf,i)[0];
        float *r = IDX(dst,sd,i);
        r[0]=fi; r[1]=0; r[2]=0; r[3]=0;
        r[4]=0; r[5]=fi; r[6]=0; r[7]=0;
        r[8]=0; r[9]=0; r[10]=fi; r[11]=0;
        r[12]=0; r[13]=0; r[14]=0; r[15]=1;
    }
}

// Mirrors MFROMF17EXPR: e[k]/se[k] are the 16 explicit-float operands (dst excluded).
void op_mfromf16(float* dst, int sd, const float* const* e, const int* se, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags,i)) {
        float *r = IDX(dst,sd,i);
        for (int k = 0; k < 16; k++) r[k] = IDX(e[k], se[k], i)[0];
    }
}

// =========================================================================
// for / while loop (JIT .slo path) — mirrors FOR3EXPR_PRE / FOREND3EXPR_PRE
// =========================================================================

void op_for_check(const float* cond, int sc,
                  int* execCount, int* tags, int n,
                  int* numActive, int* numPassive) {
    ++(*execCount);
    const int ec = *execCount;
    for (int i = 0; i < n; ++i) {
        if (tags[i] > 0) {
            tags[i]++;
        } else if (!(int)(IDX(cond, sc, i)[0])) {
            tags[i] = ec;
            --(*numActive);
            ++(*numPassive);
        }
    }
}

void op_forend(const int* execCount, int* tags, int n,
               int* numActive, int* numPassive) {
    const int ec = *execCount;
    ++g_jit_grid;
    log_debug("[JIT-forend] grid={} ec={} n={}", g_jit_grid, ec, n);
    *numActive  = 0;
    *numPassive = n;
    for (int i = 0; i < n; ++i) {
        if (tags[i] > 0) {
            tags[i] -= ec;
            if (tags[i] <= 0) { tags[i] = 0; ++(*numActive); --(*numPassive); }
        } else {
            ++(*numActive); --(*numPassive);
        }
    }
}

void op_for_break(const int* execCount, int* tags, int n,
                  int* numActive, int* numPassive) {
    const int ec = *execCount;
    for (int i = 0; i < n; ++i) {
        if (tags[i] == 0) {
            tags[i] = ec;
            ++(*numPassive);
            --(*numActive);
        }
    }
}

// =========================================================================
// illuminance loop (surface shader construct, JIT .slo path)
// =========================================================================

int op_illuminance_begin(const float* P, int sp, const float* N, int sn,
                         const float* angle, int sa,
                         int* tags, int n, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    return ctx->jitIlluminanceBegin(P, sp, N, sn, angle, sa,
                                    tags, n, numActive, numPassive);
}

int op_illuminance_next(int* tags, int n, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    return ctx->jitIlluminanceNext(tags, n, numActive, numPassive);
}

// =========================================================================
// gather / gatherElse / gatherEnd (surface shader construct, JIT .slo path)
// =========================================================================

int op_gather_begin(int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    return ctx->jitGatherBegin(numActive, numPassive);
}

int op_gather_else(int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    return ctx->jitGatherElse(numActive, numPassive);
}

int op_gather_end(int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return 0;
    return ctx->jitGatherEnd(numActive, numPassive);
}

// =========================================================================
// illuminate / endilluminate (light shader construct, JIT .slo path)
// =========================================================================

void op_illuminate_begin(const float* from, int sf,
                         int* tags, int n, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->jitIlluminateBegin(from, sf, tags, n, numActive, numPassive);
}

void op_illuminate_end(int* tags, int n, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->jitIlluminateEnd(tags, n, numActive, numPassive);
}

void op_illuminate3_begin(const float* from, int sf,
                          const float* axis, int sa,
                          const float* angle, int st,
                          int* tags, int n, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->jitIlluminate3Begin(from, sf, axis, sa, angle, st, tags, n, numActive, numPassive);
}

// =========================================================================
// solar / endsolar (directional light shader construct, JIT .slo path)
// =========================================================================

void op_solar_begin(const float* Nf, int sf, const float* thetaf, int st,
                    int* tags, int n, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->jitSolarBegin(Nf, sf, thetaf, st, tags, n, numActive, numPassive);
}

void op_solar_end(int* tags, int n, int* numActive, int* numPassive) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->jitSolarEnd(tags, n, numActive, numPassive);
}

// =========================================================================
// Layer G — additional math / comparison / built-in ops
// =========================================================================

void op_maxf(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = (IDX(a,sa,i)[0] > IDX(b,sb,i)[0]) ? IDX(a,sa,i)[0] : IDX(b,sb,i)[0];
}

void op_andf(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = (float)((int)IDX(a,sa,i)[0] & (int)IDX(b,sb,i)[0]);
}

void op_orf(float* dst, int sd, const float* a, int sa, const float* b, int sb, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = (float)((int)IDX(a,sa,i)[0] | (int)IDX(b,sb,i)[0]);
}

void op_radians(float* dst, int sd, const float* a, int sa, int n, const int* tags) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.f;
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = IDX(a,sa,i)[0] * kDegToRad;
}

void op_seql(float* dst, int sd, const char* const* a, const char* const* b, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = (a[0] && b[0] && strcmp(a[0], b[0]) == 0) ? 1.f : 0.f;
}

void op_sneql(float* dst, int sd, const char* const* a, const char* const* b, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = (a[0] && b[0] && strcmp(a[0], b[0]) != 0) ? 1.f : 0.f;
}

void op_filterstep(float* dst, int sd, const float* edge, int se, const float* x, int sx, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = (IDX(x,sx,i)[0] >= IDX(edge,se,i)[0]) ? 1.f : 0.f;
}

void op_reflect(float* dst, int sd, const float* I, int si, const float* N, int sn, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        reflect(IDX(dst,sd,i), IDX(I,si,i), IDX(N,sn,i));
}

void op_fresnel(const float* I, int si, const float* N, int sn, const float* eta, int se,
                float* Kr, int skr, float* Kt, int skt,
                float* R,  int sr,  float* T,  int st,
                int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i)) {
        float kr=0.f, kt=0.f;
        float tmpR[3]={0,0,0}, tmpT[3]={0,0,0};
        fresnel(IDX(I,si,i), IDX(N,sn,i), IDX(eta,se,i)[0], kr, kt, tmpR, tmpT);
        IDX(Kr,skr,i)[0] = kr;
        IDX(Kt,skt,i)[0] = kt;
        if (R) { IDX(R,sr,i)[0]=tmpR[0]; IDX(R,sr,i)[1]=tmpR[1]; IDX(R,sr,i)[2]=tmpR[2]; }
        if (T) { IDX(T,st,i)[0]=tmpT[0]; IDX(T,st,i)[1]=tmpT[1]; IDX(T,st,i)[2]=tmpT[2]; }
    }
}

void op_noise_ff(float* dst, int sd, const float* x, int sx, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = noiseFloat(IDX(x,sx,i)[0]);
}

void op_noise_fp(float* dst, int sd, const float* p, int sp, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = noiseFloat(IDX(p,sp,i));
}

void op_noise_vf(float* dst, int sd, const float* x, int sx, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        noiseVector(IDX(dst,sd,i), IDX(x,sx,i)[0]);
}

void op_noise_vp(float* dst, int sd, const float* p, int sp, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i)) {
        noiseVector(IDX(dst,sd,i), IDX(p,sp,i));
        if (i == 0) {
            const float *pi = IDX(p,sp,0);
            const float *di = IDX(dst,sd,0);
            log_debug("[JIT-noise-vp] in[0]=({:.4f},{:.4f},{:.4f}) out[0]=({:.4f},{:.4f},{:.4f})", pi[0],pi[1],pi[2],di[0],di[1],di[2]);
        }
    }
}

void op_cellnoise_ff(float* dst, int sd, const float* x, int sx, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = cellNoiseFloat(IDX(x,sx,i)[0]);
}
void op_cellnoise_fp(float* dst, int sd, const float* p, int sp, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = cellNoiseFloat(IDX(p,sp,i));
}
void op_cellnoise_vf(float* dst, int sd, const float* x, int sx, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        cellNoiseVector(IDX(dst,sd,i), IDX(x,sx,i)[0]);
}
void op_cellnoise_vp(float* dst, int sd, const float* p, int sp, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        cellNoiseVector(IDX(dst,sd,i), IDX(p,sp,i));
}
void op_cellnoise_fff(float* dst, int sd, const float* x, int sx, const float* y, int sy, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = cellNoiseFloat(IDX(x,sx,i)[0], IDX(y,sy,i)[0]);
}
void op_cellnoise_fpf(float* dst, int sd, const float* p, int sp, const float* f, int sf, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        IDX(dst,sd,i)[0] = cellNoiseFloat(IDX(p,sp,i), IDX(f,sf,i)[0]);
}
void op_cellnoise_vff(float* dst, int sd, const float* x, int sx, const float* y, int sy, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        cellNoiseVector(IDX(dst,sd,i), IDX(x,sx,i)[0], IDX(y,sy,i)[0]);
}
void op_cellnoise_vpf(float* dst, int sd, const float* p, int sp, const float* f, int sf, int n, const int* tags) {
    for (int i=0;i<n;i++) if(ACTIVE(tags,i))
        cellNoiseVector(IDX(dst,sd,i), IDX(p,sp,i), IDX(f,sf,i)[0]);
}

// =========================================================================
// Layer G — context-dependent geometric built-ins
// =========================================================================

void op_Du_ff(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    if (ss == 0) {
        for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0] = 0.f;
        return;
    }
    ctx->jitDuFloat(dst, src, n);
}

void op_Dv_ff(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    if (ss == 0) {
        for (int i=0;i<n;i++) if(ACTIVE(tags,i)) IDX(dst,sd,i)[0] = 0.f;
        return;
    }
    ctx->jitDvFloat(dst, src, n);
}

void op_Du_vv(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    if (ss == 0) {
        for (int i=0;i<n;i++) if(ACTIVE(tags,i)) {
            IDX(dst,sd,i)[0]=0.f; IDX(dst,sd,i)[1]=0.f; IDX(dst,sd,i)[2]=0.f;
        }
        return;
    }
    ctx->jitDuVector(dst, src, n);
}

void op_Dv_vv(float* dst, int sd, const float* src, int ss, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    if (ss == 0) {
        for (int i=0;i<n;i++) if(ACTIVE(tags,i)) {
            IDX(dst,sd,i)[0]=0.f; IDX(dst,sd,i)[1]=0.f; IDX(dst,sd,i)[2]=0.f;
        }
        return;
    }
    ctx->jitDvVector(dst, src, n);
}

void op_area(float* dst, int sd, const float* P, int /*sp*/, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->jitArea(dst, sd, P, n, tags);
    if (n > 0 && ACTIVE(tags, 0))
        log_debug("[JIT-area] P[0]=({:.4f},{:.4f},{:.4f}) area={:.6f}", P[0],P[1],P[2], IDX(dst,sd,0)[0]);
}

void op_calculatenormal(float* dst, int sd, const float* P, int /*sp*/, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    log_debug("[JIT-calculatenormal] ctx={} n={}", (void*)ctx, n);
    if (!ctx) return;
    ctx->jitCalculateNormal(dst, sd, P, n, tags);
    if (n > 0 && ACTIVE(tags, 0)) {
        const float *r0 = IDX(dst,sd,0);
        log_debug("[JIT-calculatenormal] out[0]=({:.4f},{:.4f},{:.4f})", r0[0], r0[1], r0[2]);
    }
}

void op_depth(float* dst, int sd, const float* P, int sp, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx) return;
    ctx->jitDepth(dst, sd, P, sp, n, tags);
}

// =========================================================================
// Layer G — texture / environment / shadow
// namepp is a char** (locals[idx] → char* string); namepp[0] = the string.
// =========================================================================

void op_texture_f(float* dst, int sd, const char* const* namepp, const float* chan,
                  const float* s, int ss, const float* t, int st, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx || !namepp || !namepp[0]) return;
    int channel = chan ? (int)chan[0] : 0;
    ctx->jitTextureF(dst, sd, namepp[0], channel, s, ss, t, st, n, tags);
}

void op_texture_c(float* dst, int sd, const char* const* namepp,
                  const float* s, int ss, const float* t, int st, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx || !namepp || !namepp[0]) return;
    ctx->jitTextureC(dst, sd, namepp[0], s, ss, t, st, n, tags);
}

void op_environment_f(float* dst, int sd, const char* const* namepp, const float* chan,
                      const float* D, int sD, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx || !namepp || !namepp[0]) return;
    int channel = chan ? (int)chan[0] : 0;
    ctx->jitEnvironmentF(dst, sd, namepp[0], channel, D, sD, n, tags);
}

void op_environment_c(float* dst, int sd, const char* const* namepp,
                      const float* D, int sD, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx || !namepp || !namepp[0]) return;
    ctx->jitEnvironmentC(dst, sd, namepp[0], D, sD, n, tags);
}

void op_shadow_f(float* dst, int sd, const char* const* namepp,
                 const float* Ps, int sPs, int n, const int* tags) {
    CShadingContext *ctx = libshader::activeContext();
    if (!ctx || !namepp || !namepp[0]) return;
    ctx->jitShadowF(dst, sd, namepp[0], Ps, sPs, n, tags);
}

// =========================================================================
// Spline interpolation — Catmull-Rom basis (default, no basis-string form)
// =========================================================================
// Weights derived from rslo SPLINEPEXPR: ub=[u²,u²,u,1] (ub[0]=u*ub[2]=u²),
// tmp[j] = col_j(RiCatmullRomBasis) · ub.  Expanding each column:
//   w0 = 0.5u²-0.5u,  w1 = -u²+1,  w2 = 0.5u²+0.5u,  w3 = 0  (sum=1).
static inline void catmullRomWeights(float u, float weights[4]) {
    float u2 = u * u;
    weights[0] =  0.5f*u2 - 0.5f*u;
    weights[1] = -u2 + 1.0f;
    weights[2] =  0.5f*u2 + 0.5f*u;
    weights[3] =  0.0f;
}

// Spline control points are constant-per-shader-call even when stored as varying;
// access only index [0] (and [1],[2] for RGB) from each knot pointer.
void op_spline_c(float* dst, int sd, const float* t, int st,
                 int numKnots, float** knots,
                 int n, const int* tags) {
    if (numKnots < 4) return;
    const int numPieces = numKnots - 3;
    if (n > 0) {
        float cmin = 1e30f, cmax = -1e30f;
        for (int i = 0; i < n; ++i) {
            if (tags && tags[i]) continue;
            float v = IDX(t, st, i)[0];
            if (v < cmin) cmin = v;
            if (v > cmax) cmax = v;
        }
        log_debug("[JIT-spline-c] grid={} csp[0]={:.6f} range=[{:.6f},{:.6f}]", g_jit_grid, IDX(t,st,0)[0], cmin, cmax);
    }
    for (int i = 0; i < n; ++i) {
        if (tags && tags[i]) continue;
        float *r  = IDX(dst, sd, i);
        float  tv = IDX(t, st, i)[0];
        if (tv <= 0.0f) {
            r[0] = knots[1][0]; r[1] = knots[1][1]; r[2] = knots[1][2];
        } else if (tv >= 1.0f) {
            int last = numKnots - 2;
            r[0] = knots[last][0]; r[1] = knots[last][1]; r[2] = knots[last][2];
        } else {
            int piece = (int)(tv * numPieces);
            if (piece >= numPieces) piece = numPieces - 1;
            float u = tv * numPieces - piece;
            float w[4];
            catmullRomWeights(u, w);
            r[0] = r[1] = r[2] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                r[0] += w[k] * knots[piece+k][0];
                r[1] += w[k] * knots[piece+k][1];
                r[2] += w[k] * knots[piece+k][2];
            }
        }
    }
    if (n > 0) {
        log_debug("[JIT-spline-c-out] grid={} Ct[0]=({:.4f},{:.4f},{:.4f})",
                  g_jit_grid, IDX(dst,sd,0)[0], IDX(dst,sd,0)[1], IDX(dst,sd,0)[2]);
    }
}

void op_spline_f(float* dst, int sd, const float* t, int st,
                 int numKnots, float** knots,
                 int n, const int* tags) {
    if (numKnots < 4) return;
    const int numPieces = numKnots - 3;
    for (int i = 0; i < n; ++i) {
        if (tags && tags[i]) continue;
        float *r  = IDX(dst, sd, i);
        float  tv = IDX(t, st, i)[0];
        if (tv <= 0.0f) {
            r[0] = knots[1][0];
        } else if (tv >= 1.0f) {
            r[0] = knots[numKnots-2][0];
        } else {
            int piece = (int)(tv * numPieces);
            if (piece >= numPieces) piece = numPieces - 1;
            float u = tv * numPieces - piece;
            float w[4];
            catmullRomWeights(u, w);
            r[0] = 0.0f;
            for (int k = 0; k < 4; ++k)
                r[0] += w[k] * knots[piece+k][0];
        }
    }
}

void op_ffroma(float* dst, int sd, const float* arr, int arrStride,
               const float* idx, int idxStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        const float* op1 = IDX(arr, arrStride, i);
        const float* op2 = IDX(idx, idxStride, i);
        float* res = IDX(dst, sd, i);
        *res = op1[(int)(*op2)];
    }
}

void op_vfroma(float* dst, int sd, const float* arr, int arrStride,
               const float* idx, int idxStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        const float* op1 = IDX(arr, arrStride, i);
        const float* op2 = IDX(idx, idxStride, i);
        float* res = IDX(dst, sd, i);
        movvv(res, &op1[((int)(*op2)) * 3]);
    }
}

void op_mfroma(float* dst, int sd, const float* arr, int arrStride,
               const float* idx, int idxStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        const float* op1 = IDX(arr, arrStride, i);
        const float* op2 = IDX(idx, idxStride, i);
        float* res = IDX(dst, sd, i);
        movmm(res, &op1[((int)(*op2)) * 16]);
    }
}

void op_sfroma(char** dst, int sd, const char* const* arr, int arrStride,
               const float* idx, int idxStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        const char* const* op1 = IDX(arr, arrStride, i);
        const float* op2 = IDX(idx, idxStride, i);
        char** res = IDX(dst, sd, i);
        *res = const_cast<char*>(op1[(int)(*op2)]);
    }
}

void op_ftoa(float* arr, int arrStride, const float* idx, int idxStride,
            const float* val, int valStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        float* res = IDX(arr, arrStride, i);
        const float* op1 = IDX(idx, idxStride, i);
        const float* op2 = IDX(val, valStride, i);
        res[(int)(*op1)] = *op2;
    }
}

void op_vtoa(float* arr, int arrStride, const float* idx, int idxStride,
            const float* val, int valStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        float* res = IDX(arr, arrStride, i);
        const float* op1 = IDX(idx, idxStride, i);
        const float* op2 = IDX(val, valStride, i);
        movvv(res + ((int)(*op1)) * 3, op2);
    }
}

void op_mtoa(float* arr, int arrStride, const float* idx, int idxStride,
            const float* val, int valStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        float* res = IDX(arr, arrStride, i);
        const float* op1 = IDX(idx, idxStride, i);
        const float* op2 = IDX(val, valStride, i);
        movmm(res + ((int)(*op1)) * 16, op2);
    }
}

void op_stoa(char** arr, int arrStride, const float* idx, int idxStride,
            const char* const* val, int valStride, int n, const int* tags) {
    for (int i = 0; i < n; i++) if (ACTIVE(tags, i)) {
        char** res = IDX(arr, arrStride, i);
        const float* op1 = IDX(idx, idxStride, i);
        const char* const* op2 = IDX(val, valStride, i);
        res[(int)(*op1)] = const_cast<char*>(*op2);
    }
}
