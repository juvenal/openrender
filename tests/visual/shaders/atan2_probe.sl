/* GitHub #229: the two-argument atan(y, x) is the four-quadrant arc tangent (RISpec 3.2) in both backends, for the Atan2_* tests
 * (tests/visual/atan2-scenes.cmake). The values come from parameters, so the compiler cannot fold them.
 *
 *   Ci = (atan(y, x), atan(-y, x), atan(y, -x))
 */
surface atan2_probe(float y = 1; float x = -1)
{
    Ci = color(atan(y, x), atan(-y, x), atan(y, -x));
    Oi = 1;
}
