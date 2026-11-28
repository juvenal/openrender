/* GitHub #156: a ray whose direction has an exactly zero z component missed every bilinear quad it crossed. The probe
 * shoots one ray from P to P + (dx, dy, dz) and writes the result into Ci, for the ZeroDir156_* tests
 * (tests/visual/zerodir-scenes.cmake):
 *
 *   mode "transmission":  Ci = transmission(P, Q), the product of the opacities the ray crosses (0.5 for one half-opaque sheet)
 *   mode "visibility":    Ci = visibility(P, Q), 0 when a flagged occluder lies between the points, 1 when none does
 */
surface zerodir156_probe(float dx = 3; float dy = 0; float dz = 0; string mode = "transmission")
{
    point Q = P + vector(dx, dy, dz);
    float v;
    if (mode == "visibility")
        v = visibility(P, Q);
    else
        v = comp(transmission(P, Q, "maxdist", 1e30, "bias", 0.01), 0);
    Ci = color(v, v, v);
    Oi = 1;
}
