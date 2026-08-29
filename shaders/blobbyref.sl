/**
 * blobbyref(): a solid stripe pattern read in a reference space that a
 * blobby carries per blob.
 *
 * Written for spec 015-blobby-implicit-surfaces (FR-020). A blobby has no
 * global parameterisation, so a solid texture on one is normally evaluated
 * in object or shader space -- and then the texture stays still while the
 * surface moves through it. An `mpoint` per-blob parameter gives every
 * primitive field its own map into a shared reference space, so a pattern
 * read there rides the blobs instead: bend the chain and the stripes bend
 * with it.
 *
 * The pattern is deliberately a hard-edged stripe rather than a smooth
 * gradient, so a reference space that has come loose shows up as the
 * stripes sliding rather than as a subtle shading difference.
 */

surface
blobbyref(float Ka = 0.35,
                Kd = 0.75,
                frequency = 1.4;
          color lightband = color(0.93, 0.86, 0.55),
                darkband  = color(0.32, 0.11, 0.05);
          varying point Pref = point(0, 0, 0))
{
    normal NN;
    float  band;
    color  Ct;

    NN = faceforward(normalize(N), I);

    /* Stripes along the reference space's x axis. */
    band = 0.5 + 0.5 * sin(frequency * 2 * PI * xcomp(Pref));

    Ct = mix(darkband, lightband, smoothstep(0.35, 0.65, band));

    Oi = Os;
    Ci = Os * Ct * (Ka * ambient() + Kd * diffuse(NN));
}
