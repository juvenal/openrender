/*
 * GitHub #228: the orientation probe. A point is green when the geometric normal Ng faces the viewer
 * (outward on a closed surface seen from outside) and red when it faces away; it is blue when N and Ng
 * disagree. Used by teapot-orientation-probe.rib.in through test_orient_probe, a test-only shader that is
 * compiled into the build tree and never shipped.
 */
surface orient_probe()
{
    float d = normalize(Ng) . normalize(I);
    float m = normalize(N) . normalize(Ng);
    Ci = (d < 0) ? color(0, 0.8, 0) : color(0.9, 0, 0);
    if (m < 0)
        Ci = color(0, 0, 1);
    Oi = 1;
}
