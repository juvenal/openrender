/**
 * defaultsurface(): default surface shader
 *
 * Copyright (c) 1999 PIXAR.  All rights reserved.
 */

surface
defaultsurface (float Kd = 0.8,
                      Ka = 0.2) {

    float diff;
    diff = I.N;
    diff = (diff * diff) / (I.I * N.N);

    Ci = Os * Cs * (Ka + Kd * diff);
    Oi = Os;
}
