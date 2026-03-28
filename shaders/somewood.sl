/**
 * realwood - solid-wood shader.
 *
 * Ka, Kd, Ks, specularcolor, roughness - the standard meaning.
 * grain - controls the distance between growth rings (as grain gets
 *	larger, the rings get closer).
 * swirl - the amount of turbulence in the wood pattern.
 * swirlfreq - the relative frequency of the turbulence (i.e. waves vs. 	noise).
 * c0, c1 - two points on the center line of the tree from which the
 *	wood was taken.  This shader creates concentric rings out from
 *	this line.
 * darkcolor - the color of the dark grain bands
 * darkfactor - the maximum factor the dark color is mixed in. I.e.the
 *	darkest color will be mix(Cs,darkcolor,darkfactor).
 *
 */

surface
somewood (float Ka = 1,
                Kd = 0.6,
                Ks = 0.4,
                roughness = 0.6,
                grain = 12,
                swirl = 0.1,
                swirlfreq = 0.1;
          point c0 = point "shader" (0, 0, 0),
                c1 = point "shader" (0, 0, 1);
          color specularcolor = color (1, 1, 1),
                darkcolor = color (0.67, 0.49, 0.27);
          float darkfactor = 1)
{
    point  cP, C1, C0, PP, Nf, V, newP;
    float  dd, pd, alpha, nn;
    color  Cwood;

    Nf = faceforward( normalize(N), I );
    V = -normalize(I);

    /* Compute the distance from P to the line (c0, c1). */
    C1 = transform("shader", c1);
    C0 = transform("shader", c0);
    cP = normalize(C1 - C0);
    newP = transform("shader",P);
    PP = newP - C0;
    pd = PP.cP;
    dd = sqrt(PP.PP - pd*pd);

    /* add some turbulence */
    nn = swirl*noise(swirlfreq*newP);
    nn += 0.5*swirl*noise(2.0*swirlfreq*newP);
    nn += 0.25*swirl*noise(4.0*swirlfreq*newP);
    nn += 0.125*swirl*noise(8.0*swirlfreq*newP);
    nn += 0.0625*swirl*noise(16.0*swirlfreq*newP);
    nn += 0.03125*swirl*noise(32.0*swirlfreq*newP);
    dd += nn;

    /* Compute the scale factor to be applied to the color to generate the
       grain.  The factor will vary between (1-darkfactor) and 1. */
    alpha = mod(grain * dd, 1);
    alpha *= alpha;

    /* Finally, compute the output color.  It is a specular surface scaled by
       the grain pattern. The specularity also varies with the grain:
       darker wood is more specular */
    Oi = Os;
    Cwood = mix(darkcolor, Cs, alpha);
    Ci = Os * (Cwood * (Ka * ambient() + Kd * diffuse(Nf)) +
               clamp((1 - alpha + (1 - darkfactor)), 0.75, 1) *
               Ks * specularcolor * specular(Nf, V, roughness));
}
