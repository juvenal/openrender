/**
 * volume_random_fog(): fog with a random() term (spec 056)
 *
 * fog's mix, with the density scaled by 1 - jitter*random() per point: the result depends on the point's sample
 * stream, so a thread-count determinism test of a scene that uses it means the volume ran.
 *
 * Used by the volume-random-fog scenes (examples/rib/tests).
 */
volume volume_random_fog(float distance = 1; color background = 0; float jitter = 0.5)
{
    float d = 1 - exp(-length(I) / distance);
    d = d * (1 - jitter * random());
    Ci = mix(Ci, background, d);
    Oi = mix(Oi, color(1, 1, 1), d);
}
