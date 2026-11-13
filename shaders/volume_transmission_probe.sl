/**
 * volume_transmission_probe(): a surface that calls transmission() toward a fixed point (spec 056)
 *
 * Ci = transmission(P, P + (0, 0, dz)), Oi = 1, in current (camera) space: a ray of constant length dz leaving the
 * surface into the side its direction goes to, for a scene whose surfaces are parallel to the camera plane.
 *
 * Used by the volume-transmission-interior scene (examples/rib/tests).
 */
surface volume_transmission_probe(float dz = 3)
{
    point q = P + vector(0, 0, dz);
    Ci = transmission(P, q);
    Oi = 1;
}
