/* GitHub #236 (b): a light that calls shadow("raytrace") before its illuminate() block. RISpec 3.2 table 12.2 defines L in a light only
 * inside illuminate or solar, so the ray's direction is not specified there; the result must still not depend on what an earlier
 * point's lights left in the L buffer. Cl is (1 - s, 0.5, 0.5): never zero, so the pixel can be compared. */
light shadow_prelight_light(point from = point (0, 0, -3))
{
    float s = shadow("raytrace", Ps);
    illuminate(from) {
        Cl = color(1 - s, 0.5, 0.5);
    }
}
