/**
 *
 *
 */

#include "loc_illum_ward_anisotropic.slh"

surface
anisotropicmetal (float Ka = 0.2,
                        Kd = 0.4,
                        Ks = 0.6,
                        xroughness = 0.4,
                        yroughness = 0.4)
{
    normal Nf = faceforward(normalize(N), I);
    vector V = normalize(-I);
    vector xdir = normalize(dPdu);

    Ci = Os * Cs * (Ka * ambient() + Kd * diffuse(Nf) +
                    Ks * LocIllumWardAnisotropic(Nf, V, xdir,
                                                 xroughness,
                                                 yroughness));
    Oi = Os;
}
