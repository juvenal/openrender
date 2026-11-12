/**
 * volume_gather_probe(): a surface that gathers one ray along the tilted reflection direction (spec 056)
 *
 * The same direction as volume_reflect_probe, as a cone of angle 0 with one sample, so the segment length is the same
 * for every pixel; surface:Ci of the hit is the pixel's colour. Every ray of the scene hits (the scene encloses the
 * gather point), so the else branch is never taken.
 *
 * Used by the volume-gather scene (examples/rib/tests).
 */
surface volume_gather_probe(float tiltx = 0.5)
{
    vector In = normalize(I);
    normal Nt = normal(tiltx, 0, -sqrt(1 - tiltx * tiltx));
    vector R = reflect(In, Nt);
    color c = 0;
    color tot = 0;
    gather("illuminance", P, R, 0, 1, "bias", 0.02, "maxdist", 100, "surface:Ci", c) {
        tot += c;
    }
    else {
        tot += 0;
    }
    Ci = tot;
    Oi = 1;
}
