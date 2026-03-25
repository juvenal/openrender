/**
 *
 *
 */

#include "includes/material_shiny_plastic.slh"

surface
shinyplastic (float Ka = 1,
                    Kd = 0.5,
                    Ks = 0.5,
                    roughness = 0.1,
                    Kr = 1,
                    blur = 0,
                    ior = 1.5;
                    DECLARE_DEFAULTED_ENVPARAMS) {
    normal Nf = faceforward(normalize(N), I);
    Ci = MaterialShinyPlastic(Nf, Cs, Ka, Kd, Ks, roughness, Kr, blur, ior,
                              ENVPARAMS);
    Oi = Os;
    Ci *= Oi;
}
