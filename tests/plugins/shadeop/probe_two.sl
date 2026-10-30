/*
 * Spec 037 test shader: calls both operands of the legacy shadeop fixture (probe_add and
 * probe_mul, two tables of one module). ShadeopAbi_Versions_Render counts the unversioned
 * warning across the two scans: one per module, not one per operand.
 */
surface probe_two()
{
    float v = probe_add(0.25, 0.5);
    float w = probe_mul(2.0, 0.375);
    Ci = color(v, w, v);
    Oi = Os;
}
