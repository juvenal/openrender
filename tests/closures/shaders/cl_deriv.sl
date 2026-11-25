/*
 * Spec 035, Phase 3 (T077 (4)): a probe surface that uses a derivative, so that the raytrace hider shades the derivative-offset vertices of
 * its batch (numVertices = 3 x numRealVertices) next to the real ones. ClosureMode_Both_raytrace counts the trees delivered against the real
 * points of the color render: a tree for a derivative-helper point would make the count differ (SC-002, Q11).
 */
surface cl_deriv()
{
    float a = area(P); // area() is the derivative use; it is never negative
    Ci = Cs * Os;
    if (a < 0)
        Ci = 0;
    Oi = Os;
}
