/**
 * volume_through_probe(): a surface that looks straight through itself (spec 056)
 *
 * Ci = trace(P, normalize(I)), Oi = 1, no lights. The ray leaves the surface into the side its direction goes to
 * (the interior side when the surface faces the camera), so an Interior shader attenuates the segment from this
 * point to whatever lies behind it, with no bend: the segment length is the distance between two parallel
 * surfaces, the same for every pixel.
 *
 * Used by the volume-interior-fog-slab and volume-incident-opposite scenes (examples/rib/tests).
 */
surface volume_through_probe()
{
    Ci = trace(P, normalize(I));
    Oi = 1;
}
