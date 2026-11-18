/**
 * Project: openRender
 *
 * File: photonStore.cpp
 *
 * Description:
 *   This file implements the photon store (spec 032): see photonStore.h.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	photonStore.cpp
//  Classes				:	CPhotonStore
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include <algorithm>
#include <numeric>
#include <vector>

#include "common/portable_io.h"
#include "common/diagnostics.h"
#include "common/memory.h"
#include "photonStore.h"
#include "common/random.h"
#include "common/stats.h"

// Precomputed conversion tables for the sin/cosine
const float costheta[] = {1.000000f, 0.999925f, 0.999699f, 0.999322f, 0.998795f, 0.998118f, 0.997290f, 0.996313f, 0.995185f, 0.993907f, 0.992480f, 0.990903f, 0.989177f, 0.987301f, 0.985278f, 0.983105f, 0.980785f, 0.978317f, 0.975702f, 0.972940f, 0.970031f, 0.966976f, 0.963776f, 0.960431f, 0.956940f, 0.953306f, 0.949528f, 0.945607f, 0.941544f, 0.937339f, 0.932993f, 0.928506f, 0.923880f, 0.919114f, 0.914210f, 0.909168f, 0.903989f, 0.898674f, 0.893224f, 0.887640f, 0.881921f, 0.876070f, 0.870087f, 0.863973f, 0.857729f, 0.851355f, 0.844854f, 0.838225f, 0.831470f, 0.824589f, 0.817585f, 0.810457f, 0.803208f, 0.795837f, 0.788346f, 0.780737f, 0.773010f, 0.765167f, 0.757209f, 0.749136f, 0.740951f, 0.732654f, 0.724247f, 0.715731f, 0.707107f, 0.698376f, 0.689541f, 0.680601f, 0.671559f, 0.662416f, 0.653173f, 0.643832f, 0.634393f, 0.624859f, 0.615232f, 0.605511f, 0.595699f, 0.585798f, 0.575808f, 0.565732f, 0.555570f, 0.545325f, 0.534998f, 0.524590f, 0.514103f, 0.503538f, 0.492898f, 0.482184f, 0.471397f, 0.460539f, 0.449611f, 0.438616f, 0.427555f, 0.416430f, 0.405241f, 0.393992f, 0.382683f, 0.371317f, 0.359895f, 0.348419f, 0.336890f, 0.325310f, 0.313682f, 0.302006f, 0.290285f, 0.278520f, 0.266713f, 0.254866f, 0.242980f, 0.231058f, 0.219101f, 0.207111f, 0.195090f, 0.183040f, 0.170962f, 0.158858f, 0.146730f, 0.134581f, 0.122411f, 0.110222f, 0.098017f, 0.085797f, 0.073565f, 0.061321f, 0.049068f, 0.036807f, 0.024541f, 0.012272f, 0.000000f, -0.012272f, -0.024541f, -0.036807f, -0.049068f, -0.061321f, -0.073565f, -0.085797f, -0.098017f, -0.110222f, -0.122411f, -0.134581f, -0.146730f, -0.158858f, -0.170962f, -0.183040f, -0.195090f, -0.207111f, -0.219101f, -0.231058f, -0.242980f, -0.254866f, -0.266713f, -0.278520f, -0.290285f, -0.302006f, -0.313682f, -0.325310f, -0.336890f, -0.348419f, -0.359895f, -0.371317f, -0.382683f, -0.393992f, -0.405241f, -0.416430f, -0.427555f, -0.438616f, -0.449611f, -0.460539f, -0.471397f, -0.482184f, -0.492898f, -0.503538f, -0.514103f, -0.524590f, -0.534998f, -0.545325f, -0.555570f, -0.565732f, -0.575808f, -0.585798f, -0.595699f, -0.605511f, -0.615232f, -0.624859f, -0.634393f, -0.643832f, -0.653173f, -0.662416f, -0.671559f, -0.680601f, -0.689541f, -0.698376f, -0.707107f, -0.715731f, -0.724247f, -0.732654f, -0.740951f, -0.749136f, -0.757209f, -0.765167f, -0.773010f, -0.780737f, -0.788346f, -0.795837f, -0.803208f, -0.810457f, -0.817585f, -0.824589f, -0.831470f, -0.838225f, -0.844854f, -0.851355f, -0.857729f, -0.863973f, -0.870087f, -0.876070f, -0.881921f, -0.887640f, -0.893224f, -0.898674f, -0.903989f, -0.909168f, -0.914210f, -0.919114f, -0.923880f, -0.928506f, -0.932993f, -0.937339f, -0.941544f, -0.945607f, -0.949528f, -0.953306f, -0.956940f, -0.960431f, -0.963776f, -0.966976f, -0.970031f, -0.972940f, -0.975702f, -0.978317f, -0.980785f, -0.983105f, -0.985278f, -0.987301f, -0.989177f, -0.990903f, -0.992480f, -0.993907f, -0.995185f, -0.996313f, -0.997290f, -0.998118f, -0.998795f, -0.999322f, -0.999699f, -0.999925f};
const float sintheta[] = {0.000000f, 0.012272f, 0.024541f, 0.036807f, 0.049068f, 0.061321f, 0.073565f, 0.085797f, 0.098017f, 0.110222f, 0.122411f, 0.134581f, 0.146730f, 0.158858f, 0.170962f, 0.183040f, 0.195090f, 0.207111f, 0.219101f, 0.231058f, 0.242980f, 0.254866f, 0.266713f, 0.278520f, 0.290285f, 0.302006f, 0.313682f, 0.325310f, 0.336890f, 0.348419f, 0.359895f, 0.371317f, 0.382683f, 0.393992f, 0.405241f, 0.416430f, 0.427555f, 0.438616f, 0.449611f, 0.460539f, 0.471397f, 0.482184f, 0.492898f, 0.503538f, 0.514103f, 0.524590f, 0.534998f, 0.545325f, 0.555570f, 0.565732f, 0.575808f, 0.585798f, 0.595699f, 0.605511f, 0.615232f, 0.624859f, 0.634393f, 0.643832f, 0.653173f, 0.662416f, 0.671559f, 0.680601f, 0.689541f, 0.698376f, 0.707107f, 0.715731f, 0.724247f, 0.732654f, 0.740951f, 0.749136f, 0.757209f, 0.765167f, 0.773010f, 0.780737f, 0.788346f, 0.795837f, 0.803208f, 0.810457f, 0.817585f, 0.824589f, 0.831470f, 0.838225f, 0.844854f, 0.851355f, 0.857729f, 0.863973f, 0.870087f, 0.876070f, 0.881921f, 0.887640f, 0.893224f, 0.898674f, 0.903989f, 0.909168f, 0.914210f, 0.919114f, 0.923880f, 0.928506f, 0.932993f, 0.937339f, 0.941544f, 0.945607f, 0.949528f, 0.953306f, 0.956940f, 0.960431f, 0.963776f, 0.966976f, 0.970031f, 0.972940f, 0.975702f, 0.978317f, 0.980785f, 0.983105f, 0.985278f, 0.987301f, 0.989177f, 0.990903f, 0.992480f, 0.993907f, 0.995185f, 0.996313f, 0.997290f, 0.998118f, 0.998795f, 0.999322f, 0.999699f, 0.999925f, 1.000000f, 0.999925f, 0.999699f, 0.999322f, 0.998795f, 0.998118f, 0.997290f, 0.996313f, 0.995185f, 0.993907f, 0.992480f, 0.990903f, 0.989177f, 0.987301f, 0.985278f, 0.983105f, 0.980785f, 0.978317f, 0.975702f, 0.972940f, 0.970031f, 0.966976f, 0.963776f, 0.960431f, 0.956940f, 0.953306f, 0.949528f, 0.945607f, 0.941544f, 0.937339f, 0.932993f, 0.928506f, 0.923880f, 0.919114f, 0.914210f, 0.909168f, 0.903989f, 0.898674f, 0.893224f, 0.887640f, 0.881921f, 0.876070f, 0.870087f, 0.863973f, 0.857729f, 0.851355f, 0.844854f, 0.838225f, 0.831470f, 0.824589f, 0.817585f, 0.810457f, 0.803208f, 0.795837f, 0.788346f, 0.780737f, 0.773010f, 0.765167f, 0.757209f, 0.749136f, 0.740951f, 0.732654f, 0.724247f, 0.715731f, 0.707107f, 0.698376f, 0.689541f, 0.680601f, 0.671559f, 0.662416f, 0.653173f, 0.643832f, 0.634393f, 0.624859f, 0.615232f, 0.605511f, 0.595699f, 0.585798f, 0.575808f, 0.565732f, 0.555570f, 0.545325f, 0.534998f, 0.524590f, 0.514103f, 0.503538f, 0.492898f, 0.482184f, 0.471397f, 0.460539f, 0.449611f, 0.438616f, 0.427555f, 0.416430f, 0.405241f, 0.393992f, 0.382683f, 0.371317f, 0.359895f, 0.348419f, 0.336890f, 0.325310f, 0.313682f, 0.302006f, 0.290285f, 0.278520f, 0.266713f, 0.254866f, 0.242980f, 0.231058f, 0.219101f, 0.207111f, 0.195090f, 0.183040f, 0.170962f, 0.158858f, 0.146730f, 0.134581f, 0.122411f, 0.110222f, 0.098017f, 0.085797f, 0.073565f, 0.061321f, 0.049068f, 0.036807f, 0.024541f, 0.012272f};
const float cosphi[] = {1.000000f, 0.999699f, 0.998795f, 0.997290f, 0.995185f, 0.992480f, 0.989177f, 0.985278f, 0.980785f, 0.975702f, 0.970031f, 0.963776f, 0.956940f, 0.949528f, 0.941544f, 0.932993f, 0.923880f, 0.914210f, 0.903989f, 0.893224f, 0.881921f, 0.870087f, 0.857729f, 0.844854f, 0.831470f, 0.817585f, 0.803208f, 0.788346f, 0.773010f, 0.757209f, 0.740951f, 0.724247f, 0.707107f, 0.689541f, 0.671559f, 0.653173f, 0.634393f, 0.615232f, 0.595699f, 0.575808f, 0.555570f, 0.534998f, 0.514103f, 0.492898f, 0.471397f, 0.449611f, 0.427555f, 0.405241f, 0.382683f, 0.359895f, 0.336890f, 0.313682f, 0.290285f, 0.266713f, 0.242980f, 0.219101f, 0.195090f, 0.170962f, 0.146730f, 0.122411f, 0.098017f, 0.073565f, 0.049068f, 0.024541f, 0.000000f, -0.024541f, -0.049068f, -0.073565f, -0.098017f, -0.122411f, -0.146730f, -0.170962f, -0.195090f, -0.219101f, -0.242980f, -0.266713f, -0.290285f, -0.313682f, -0.336890f, -0.359895f, -0.382683f, -0.405241f, -0.427555f, -0.449611f, -0.471397f, -0.492898f, -0.514103f, -0.534998f, -0.555570f, -0.575808f, -0.595699f, -0.615232f, -0.634393f, -0.653173f, -0.671559f, -0.689541f, -0.707107f, -0.724247f, -0.740951f, -0.757209f, -0.773010f, -0.788346f, -0.803208f, -0.817585f, -0.831470f, -0.844854f, -0.857729f, -0.870087f, -0.881921f, -0.893224f, -0.903989f, -0.914210f, -0.923880f, -0.932993f, -0.941544f, -0.949528f, -0.956940f, -0.963776f, -0.970031f, -0.975702f, -0.980785f, -0.985278f, -0.989177f, -0.992480f, -0.995185f, -0.997290f, -0.998795f, -0.999699f, -1.000000f, -0.999699f, -0.998795f, -0.997290f, -0.995185f, -0.992480f, -0.989177f, -0.985278f, -0.980785f, -0.975702f, -0.970031f, -0.963776f, -0.956940f, -0.949528f, -0.941544f, -0.932993f, -0.923880f, -0.914210f, -0.903989f, -0.893224f, -0.881921f, -0.870087f, -0.857729f, -0.844854f, -0.831470f, -0.817585f, -0.803208f, -0.788346f, -0.773010f, -0.757209f, -0.740951f, -0.724247f, -0.707107f, -0.689541f, -0.671559f, -0.653173f, -0.634393f, -0.615232f, -0.595699f, -0.575808f, -0.555570f, -0.534998f, -0.514103f, -0.492898f, -0.471397f, -0.449611f, -0.427555f, -0.405241f, -0.382683f, -0.359895f, -0.336890f, -0.313682f, -0.290285f, -0.266713f, -0.242980f, -0.219101f, -0.195090f, -0.170962f, -0.146730f, -0.122411f, -0.098017f, -0.073565f, -0.049068f, -0.024541f, -0.000000f, 0.024541f, 0.049068f, 0.073565f, 0.098017f, 0.122411f, 0.146730f, 0.170962f, 0.195090f, 0.219101f, 0.242980f, 0.266713f, 0.290285f, 0.313682f, 0.336890f, 0.359895f, 0.382683f, 0.405241f, 0.427555f, 0.449611f, 0.471397f, 0.492898f, 0.514103f, 0.534998f, 0.555570f, 0.575808f, 0.595699f, 0.615232f, 0.634393f, 0.653173f, 0.671559f, 0.689541f, 0.707107f, 0.724247f, 0.740951f, 0.757209f, 0.773010f, 0.788346f, 0.803208f, 0.817585f, 0.831470f, 0.844854f, 0.857729f, 0.870087f, 0.881921f, 0.893224f, 0.903989f, 0.914210f, 0.923880f, 0.932993f, 0.941544f, 0.949528f, 0.956940f, 0.963776f, 0.970031f, 0.975702f, 0.980785f, 0.985278f, 0.989177f, 0.992480f, 0.995185f, 0.997290f, 0.998795f, 0.999699f};
const float sinphi[] = {0.000000f, 0.024541f, 0.049068f, 0.073565f, 0.098017f, 0.122411f, 0.146730f, 0.170962f, 0.195090f, 0.219101f, 0.242980f, 0.266713f, 0.290285f, 0.313682f, 0.336890f, 0.359895f, 0.382683f, 0.405241f, 0.427555f, 0.449611f, 0.471397f, 0.492898f, 0.514103f, 0.534998f, 0.555570f, 0.575808f, 0.595699f, 0.615232f, 0.634393f, 0.653173f, 0.671559f, 0.689541f, 0.707107f, 0.724247f, 0.740951f, 0.757209f, 0.773010f, 0.788346f, 0.803208f, 0.817585f, 0.831470f, 0.844854f, 0.857729f, 0.870087f, 0.881921f, 0.893224f, 0.903989f, 0.914210f, 0.923880f, 0.932993f, 0.941544f, 0.949528f, 0.956940f, 0.963776f, 0.970031f, 0.975702f, 0.980785f, 0.985278f, 0.989177f, 0.992480f, 0.995185f, 0.997290f, 0.998795f, 0.999699f, 1.000000f, 0.999699f, 0.998795f, 0.997290f, 0.995185f, 0.992480f, 0.989177f, 0.985278f, 0.980785f, 0.975702f, 0.970031f, 0.963776f, 0.956940f, 0.949528f, 0.941544f, 0.932993f, 0.923880f, 0.914210f, 0.903989f, 0.893224f, 0.881921f, 0.870087f, 0.857729f, 0.844854f, 0.831470f, 0.817585f, 0.803208f, 0.788346f, 0.773010f, 0.757209f, 0.740951f, 0.724247f, 0.707107f, 0.689541f, 0.671559f, 0.653173f, 0.634393f, 0.615232f, 0.595699f, 0.575808f, 0.555570f, 0.534998f, 0.514103f, 0.492898f, 0.471397f, 0.449611f, 0.427555f, 0.405241f, 0.382683f, 0.359895f, 0.336890f, 0.313682f, 0.290285f, 0.266713f, 0.242980f, 0.219101f, 0.195090f, 0.170962f, 0.146730f, 0.122411f, 0.098017f, 0.073565f, 0.049068f, 0.024541f, 0.000000f, -0.024541f, -0.049068f, -0.073565f, -0.098017f, -0.122411f, -0.146730f, -0.170962f, -0.195090f, -0.219101f, -0.242980f, -0.266713f, -0.290285f, -0.313682f, -0.336890f, -0.359895f, -0.382683f, -0.405241f, -0.427555f, -0.449611f, -0.471397f, -0.492898f, -0.514103f, -0.534998f, -0.555570f, -0.575808f, -0.595699f, -0.615232f, -0.634393f, -0.653173f, -0.671559f, -0.689541f, -0.707107f, -0.724247f, -0.740951f, -0.757209f, -0.773010f, -0.788346f, -0.803208f, -0.817585f, -0.831470f, -0.844854f, -0.857729f, -0.870087f, -0.881921f, -0.893224f, -0.903989f, -0.914210f, -0.923880f, -0.932993f, -0.941544f, -0.949528f, -0.956940f, -0.963776f, -0.970031f, -0.975702f, -0.980785f, -0.985278f, -0.989177f, -0.992480f, -0.995185f, -0.997290f, -0.998795f, -0.999699f, -1.000000f, -0.999699f, -0.998795f, -0.997290f, -0.995185f, -0.992480f, -0.989177f, -0.985278f, -0.980785f, -0.975702f, -0.970031f, -0.963776f, -0.956940f, -0.949528f, -0.941544f, -0.932993f, -0.923880f, -0.914210f, -0.903989f, -0.893224f, -0.881921f, -0.870087f, -0.857729f, -0.844854f, -0.831470f, -0.817585f, -0.803208f, -0.788346f, -0.773010f, -0.757209f, -0.740951f, -0.724247f, -0.707107f, -0.689541f, -0.671559f, -0.653173f, -0.634393f, -0.615232f, -0.595699f, -0.575808f, -0.555570f, -0.534998f, -0.514103f, -0.492898f, -0.471397f, -0.449611f, -0.427555f, -0.405241f, -0.382683f, -0.359895f, -0.336890f, -0.313682f, -0.290285f, -0.266713f, -0.242980f, -0.219101f, -0.195090f, -0.170962f, -0.146730f, -0.122411f, -0.098017f, -0.073565f, -0.049068f, -0.024541f};

#define PHOTON_CAUSTICS 1
#define PHOTON_EXPLOSION 2
#define PHOTON_POINT 4
#define PHOTON_DISTANT 8
#define PHOTON_AREA 16

///////////////////////////////////////////////////////////////////////
//
//
//	Regular Photon Mapping stuff
//
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	CPhotonStore
// Description			:	Ctor
// Return Value			:
// Comments				:
CPhotonStore::CPhotonStore(const char *n, FILE *in, const float *curFromWorld, const float *curToWorld) : CMap<CPhoton>(), CFileResource(n) {

    keys = NULL;
    keysCapacity = 0;
    modifying = FALSE;
    maxPower = 0;
    osCreateMutex(mutex);

    identitym(from);
    identitym(to);

    // Try to read the photonmap
    if (in != NULL) {
        matrix fileFromWorld, fileToWorld;

        CMap<CPhoton>::read(in);
        ensureKeys(); // no file carries a key: every photon read has key 0

        // Read the transformation matrices (portable I/O - Phase 2)
        if (!readFloat32Array(in, fileFromWorld, 16)) {
            error(CODE_SYSTEM, "Failed to read photon map fromWorld matrix\n");
            return;
        }
        if (!readFloat32Array(in, fileToWorld, 16)) {
            error(CODE_SYSTEM, "Failed to read photon map toWorld matrix\n");
            return;
        }

        float32_p maxPower_f;
        if (!readFloat32(in, maxPower_f)) {
            error(CODE_SYSTEM, "Failed to read photon map maxPower\n");
            return;
        }
        maxPower = maxPower_f;

        mulmm(to, fileFromWorld, curToWorld);
        mulmm(from, curFromWorld, fileToWorld);
    }
    else {
        // Balance the (empty) map: an empty map gets its dummy photon
        balance();
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	~CPhotonStore
// Description			:	Dtor
// Return Value			:
// Comments				:
CPhotonStore::~CPhotonStore() {

    osDeleteMutex(mutex);

    delete[] keys;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	reset
// Description			:	Reset the photonmap
// Return Value			:
// Comments				:
void CPhotonStore::reset() {
    CMap<CPhoton>::reset(); // items, counts and the bound

    delete[] keys;
    keys = NULL;
    keysCapacity = 0;

    // The power of a map read from a file is the square root of an earlier frame's: left in place it made the next pass's maxPower
    // drift (research.md R1's second cause)
    maxPower = 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	write
// Description			:	Write the photon map into a file
// Return Value			:
// Comments				:
void CPhotonStore::write(const float *fromWorld, const float *toWorld) {
    // Note: write is multithread safe, though it will
    // only ever be called in a single-threaded state
    // we do this so it manages the modifying state properly

    if (modifying == TRUE) {
        // Flush the photonmap
        FILE *out = ropen(name, "wb", filePhotonMap);

        if (out != NULL) {

            // Balance the map
            balance();

            // Every byte of the file is defined (FR-004): rebuild each item field by field in a zeroed buffer and copy it back, so the
            // padding and item 0 are zero whatever the compiler's struct copies left in them
            {
                CPhoton *clean = new CPhoton[numItems + 1]();
                for (int i = 1; i <= numItems; i++) {
                    movvv(clean[i].P, items[i].P);
                    movvv(clean[i].N, items[i].N);
                    clean[i].flags = items[i].flags;
                    movvv(clean[i].C, items[i].C);
                    clean[i].theta = items[i].theta;
                    clean[i].phi = items[i].phi;
                }
                memcpy((void *)items, (const void *)clean, sizeof(CPhoton) * (size_t)(numItems + 1));
                delete[] clean;
            }

            // Write the map
            CMap<CPhoton>::write(out);

            // Write the matrices (portable I/O - Phase 2)
            if (!writeFloat32Array(out, fromWorld, 16)) {
                error(CODE_SYSTEM, "Failed to write photon map fromWorld matrix\n");
                fclose(out);
                return;
            }
            if (!writeFloat32Array(out, toWorld, 16)) {
                error(CODE_SYSTEM, "Failed to write photon map toWorld matrix\n");
                fclose(out);
                return;
            }

            if (!writeFloat32(out, static_cast<float32_p>(maxPower))) {
                error(CODE_SYSTEM, "Failed to write photon map maxPower\n");
                fclose(out);
                return;
            }

            fclose(out);
        }
        else {
            error(CODE_BADFILE, "Failed to open \"%s\" for writing\n", name);
        }

        // We are no longer modifying this map
        modifying = FALSE;
    }
}


///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	lookup
// Description			:	Locate the nearest maxFoundPhoton photons
// Return Value			:
// Comments				:	Nl	must be normalized
//							Il	must be normalized
void CPhotonStore::lookup(float *Cl, const float *Pl, const float *Nl, int maxFound) {
    int numFound;
    const CPhoton **indices = (const CPhoton **)alloca((maxFound + 1) * sizeof(CPhoton *));
    float *distances = (float *)alloca((maxFound + 1) * sizeof(float));
    CLookup l;

    // A local: a lookup writes no state another lookup reads (FR-005)
    const float searchRadius = (sqrtf(maxFound * maxPower / 0.05f) / (float)C_PI) * 0.5f;

    // Lookup the photonmap at the location of the photon
    distances[0] = searchRadius * searchRadius;
    l.maxFound = maxFound;
    l.numFound = 0;
    mulmp(l.P, to, Pl);
    mulmn(l.N, from, Nl);
    l.gotHeap = FALSE;
    l.indices = indices;
    l.distances = distances;

    CMap<CPhoton>::lookupWithN(&l, 1);

    initv(Cl, 0);

    if ((numFound = l.numFound) < 2)
        return;

    // Accumulate the irradiance
    for (int i = 1; i <= numFound; i++) {
        const CPhoton *p = indices[i];
        vector I;

        assert(distances[i] <= distances[0]);

        itemToDir(I, p->theta, p->phi);

        if (dotvv(I, l.N) < 0) {
            addvv(Cl, p->C);
        }
    }

    // Normalize the result
    mulvf(Cl, (float)(1.0 / (C_PI * distances[0])));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	lookup
// Description			:	Locate the nearest maxFoundPhoton photons
// Return Value			:
// Comments				:	Nl	must be normalized
//							Il	must be normalized
void CPhotonStore::lookup(float *Cl, const float *Pl, int maxFound) {
    int numFound = 0;
    const CPhoton **indices = (const CPhoton **)alloca((maxFound + 1) * sizeof(CPhoton *));
    float *distances = (float *)alloca((maxFound + 1) * sizeof(float));
    CLookup l;

    // A local: a lookup writes no state another lookup reads (FR-005)
    const float searchRadius = (sqrtf(maxFound * maxPower / 0.05f) / (float)C_PI) * 0.5f;

    distances[0] = searchRadius * searchRadius;

    l.maxFound = maxFound;
    l.numFound = 0;
    mulmp(l.P, to, Pl);
    initv(l.N, 0, 0, 0);
    l.gotHeap = FALSE;
    l.indices = indices;
    l.distances = distances;

    CMap<CPhoton>::lookup(&l, 1);

    initv(Cl, 0, 0, 0);

    if (l.numFound < 2)
        return;

    numFound = l.numFound;

    for (int i = 1; i <= numFound; i++) {
        const CPhoton *p = indices[i];

        assert(distances[i] <= distances[0]);

        addvv(Cl, p->C);
    }

    // Normalize the result
    mulvf(Cl, (float)(1.0 / (C_PI * distances[0])));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	balance
// Description			:	Balance the map
// Return Value			:
// Comments				:
void CPhotonStore::balance() {
    // If we have no photons in the map, add a dummy one to avoid an if statement during the lookup
    if (numItems == 0) {
        vector P = {0, 0, 0};
        vector I = {0, 0, 1};
        CPhoton *photon = CMap<CPhoton>::store(P, I);
        initv(photon->C, 0, 0, 0);
        photon->theta = 0;
        photon->phi = 0;
        ensureKeys();
        keys[numItems] = 0;
    }

    // The canonical photon order (FR-002, data-model.md section 1): items 1..numItems and their keys sorted by (key, then the bytes of
    // P, N, C, theta, phi as unsigned bytes: 38 bytes; `flags`, which balance() assigns, is not part of a photon's identity). Two photons
    // that compare equal are byte-identical, so their relative order cannot change a byte of the map. balance() below then runs on this
    // order: its median partition, the tie between equal coordinates and every `flags` value are functions of the photon set alone,
    // whatever order the threads stored the photons in
    ensureKeys();
    if (numItems > 1) {
        std::vector<int> order((size_t)numItems);
        std::iota(order.begin(), order.end(), 1);
        const CPhoton *base = items;
        const uint64_t *k = keys;
        std::sort(order.begin(), order.end(), [base, k](int a, int b) {
            if (k[a] != k[b]) {
                return k[a] < k[b];
            }
            const CPhoton &x = base[a], &y = base[b];
            int c = memcmp(x.P, y.P, sizeof(x.P));
            if (c == 0)
                c = memcmp(x.N, y.N, sizeof(x.N));
            if (c == 0)
                c = memcmp(x.C, y.C, sizeof(x.C));
            if (c == 0)
                c = (int)x.theta - (int)y.theta;
            if (c == 0)
                c = (int)x.phi - (int)y.phi;
            return c < 0;
        });
        CPhoton *sortedItems = new CPhoton[maxItems + 1]();
        uint64_t *sortedKeys = new uint64_t[maxItems + 1]();
        for (int i = 0; i < numItems; i++) {
            sortedItems[i + 1] = items[order[(size_t)i]];
            sortedKeys[i + 1] = keys[order[(size_t)i]];
        }
        delete[] items;
        items = sortedItems;
        delete[] keys;
        keys = sortedKeys;
        keysCapacity = maxItems + 1;
    }

    CMap<CPhoton>::balance();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	store
// Description			:	Store a photon
// Return Value			:
// Comments				:
void CPhotonStore::store(const float *P, const float *N, const float *I, const float *C) {
    store(P, N, I, C, 0);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	store
// Description			:	Store a photon with the key of the point that emitted it
// Return Value			:
// Comments				:	The key is kept in the side array, under the same lock as the photon
void CPhotonStore::store(const float *P, const float *N, const float *I, const float *C, uint64_t key) {
    osLock(mutex);
    storeLocked(P, N, I, C, key);
    osUnlock(mutex);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	storePhotonForFrame
// Description			:	Store a photon of the current photon frame; the first call of a frame resets the map
// Return Value			:	TRUE for that first call only
// Comments				:	The test, the reset, the mark and the store are one critical section: exactly one caller of a frame sees
//							TRUE (it puts the map on its balance list) and no store can run between the claim and the reset (FR-003,
//							GitHub #164). write() clears `modifying` again, so the next frame claims anew
bool CPhotonStore::storePhotonForFrame(const float *P, const float *N, const float *I, const float *C, uint64_t key) {
    bool claimed = false;

    osLock(mutex);
    if (modifying != TRUE) {
        reset();
        modifying = TRUE;
        claimed = true;
    }
    storeLocked(P, N, I, C, key);
    osUnlock(mutex);

    return claimed;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	storeLocked
// Description			:	Store a photon; the caller holds the lock
// Return Value			:
// Comments				:
void CPhotonStore::storeLocked(const float *P, const float *N, const float *I, const float *C, uint64_t key) {
    CPhoton *ton = CMap<CPhoton>::store(P, N);
    ensureKeys();
    keys[numItems] = key;
    dirToItem(ton->theta, ton->phi, I);
    movvv(ton->C, C);
    float dotCC = dotvv(C, C);
    // maxPower is the largest photon power itself (the file holds the same number): a lookup of an in-memory map and of the map
    // reloaded from its file use the same search radius (FR-005, SC-005). It used to hold the squared power until write() took the
    // square root in place
    const float power = sqrtf(dotCC);
    if (power > maxPower) {
        maxPower = power;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	ensureKeys
// Description			:	Grow the side array of keys to cover every item the map can hold
// Return Value			:
// Comments				:	New entries are 0. The caller holds the lock when other threads may store
void CPhotonStore::ensureKeys() {
    if (keysCapacity >= maxItems + 1)
        return;

    uint64_t *newKeys = new uint64_t[maxItems + 1]();
    for (int i = 0; i < keysCapacity; i++)
        newKeys[i] = keys[i];
    delete[] keys;
    keys = newKeys;
    keysCapacity = maxItems + 1;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	permuted
// Description			:	balance() reordered the photons: reorder the keys the same way
// Return Value			:
// Comments				:
void CPhotonStore::permuted(CPhoton *const *sorted, const CPhoton *oldItems, int count) {
    ensureKeys();

    uint64_t *newKeys = new uint64_t[count + 1]();
    for (int i = 1; i <= count; i++)
        newKeys[i] = keys[sorted[i] - oldItems];

    delete[] keys;
    keys = newKeys;
    keysCapacity = count + 1;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonStore
// Method				:	bound
// Description			:	Bound the data
// Return Value			:
// Comments				:
void CPhotonStore::bound(float *bmin, float *bmax) const {
    int i;

    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);

    for (i = 1; i < numItems; i++)
        addBox(bmin, bmax, items[i].P);
}
