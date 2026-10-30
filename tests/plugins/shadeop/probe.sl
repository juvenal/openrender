/*
 * Spec 037 test shader: calls the probe_add operand of the shadeop fixtures
 * (shadeop_fixture.cpp). The result is the operand's sum, 0.75 when it ran.
 */
surface probe()
{
    float v = probe_add(0.25, 0.5);
    Ci = color(v, v, v);
    Oi = Os;
}
