/*
 * uvgradient.sl -- imager shader that maps raster P to Ci for testing.
 *
 * DESCRIPTION:
 *   Maps raster-space P into Ci: Ci.r = P.x, Ci.g = P.y, Ci.b = 0.
 *   Used to verify that P, ncomps, time, and dtime variables are correctly
 *   populated by CImagerExecutor.
 *
 * PARAMETERS:
 *   xresolution  - image width in pixels (used to normalise P.x)
 *   yresolution  - image height in pixels (used to normalise P.y)
 *
 *   With defaults (1,1), Ci = (P.x, P.y, 0) — raw raster coordinates.
 */

imager
uvgradient (float xresolution = 1, yresolution = 1)
{
    Ci = color(P[0] / xresolution, P[1] / yresolution, 0);
    Oi = 1;
    alpha = 1;
}
