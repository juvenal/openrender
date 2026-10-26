/* GitHub #55: the hit shader of the ShadingParity_JitPL* trace scenes. A
 * point is shaded by the label of the ray that reached it, so a trace() whose
 * "label" parameter is lost, or that carries a label left behind by an
 * earlier gather(), renders a different value:
 *
 *   "pl55" 0.9    "" 0.3    "gather" 0.6    "camera" 0.1    anything else 0.75
 */
surface jit_pl_label_probe()
{
    uniform string l = raylabel();
    varying float v = 0.75;
    if (l == "pl55")
        v = 0.9;
    if (l == "")
        v = 0.3;
    if (l == "gather")
        v = 0.6;
    if (l == "camera")
        v = 0.1;
    Ci = color(v, v, v);
    Oi = 1;
}
