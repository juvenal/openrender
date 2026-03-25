/**
 *
 *
 */

#include "includes/material_shiny_metal.slh"

surface
shinymetal (float Ka = 1,
                  Kd = 0.1,
                  Ks = 1,
                  roughness = 0.2,
                  Kr = 0.8,
                  blur = 0;
            DECLARE_DEFAULTED_ENVPARAMS) {
    
    normal Nf = faceforward(normalize(N), I);
    Ci = MaterialShinyMetal(Nf, Cs, Ka, Kd, Ks, roughness, Kr, blur,
                            ENVPARAMS);
    Oi = Os;
    Ci *= Oi;
}
