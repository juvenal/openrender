/* GitHub #236: shadow("raytrace", P) called from a surface, with no light running for the point. The ray of a traced shadow() is
 * built from the light state (L) that a light's illuminate() leaves behind, so outside a light the call has nothing to trace
 * toward: it must answer "not shadowed", RISpec 3.2 section 15.7.3 (0 is completely illuminated), not read whatever the L buffer
 * holds. Ci is (float form, color form red, color form blue) and every channel must be exactly 0. */
surface shadow_nolight_surface()
{
    float s = shadow("raytrace", P);
    color c = shadow("raytrace", P);
    Ci = color(s, comp(c, 0), comp(c, 2));
    Oi = 1;
}
