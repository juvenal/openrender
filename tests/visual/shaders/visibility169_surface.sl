/* GitHub #169: visibility(P1, P2) is 1 when nothing flagged transmission-visible lies between the points and 0 when
 * something does, for the Visibility169_* tests (tests/visual/visibility-scenes.cmake). The segment runs from P along +z
 * for dist units, so a scene decides what lies on it by where it puts its disks.
 *
 *   Ci = (v, v, v), v = visibility(P, P + (0, 0, dist))
 */
surface visibility169_surface(float dist = 2)
{
    float v = visibility(P, P + vector(0, 0, dist));
    Ci = color(v, v, v);
    Oi = 1;
}
