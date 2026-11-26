/* GitHub #236: as shadow_nolight_surface, but after an illuminance loop has run and left the last light's L in the buffer. Outside
 * the loop the call is still outside any light: Ci must be 0 in every channel, not a ray toward the last light. Channel 2 holds the
 * loop's own count of lights (1), so the scene proves the loop ran. */
surface shadow_afterloop_surface()
{
    float n = 0;
    illuminance(P) { n += 1; }
    float s = shadow("raytrace", P);
    color c = shadow("raytrace", P);
    Ci = color(s, comp(c, 1), n);
    Oi = 1;
}
