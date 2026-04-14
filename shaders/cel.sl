/**
 *
 *
 *
 *
 *
 *
 */

#include "material_cel.slh"

 surface
 cel (float Ka = 1,
            Kd = 1,
            Ks = 0.5,
            roughness = 0.25,
            outlinethickness = 1)

{
    normal Nf = faceforward(normalize(N), I);
    Ci = MaterialCel(Nf, Cs, Ka, Kd, Ks, roughness, 0.25);
    Oi = Os;
    Ci *= Oi;
    Ci = CelOutline(N, outlinethickness);
}