/* R14a probe 1 (spec 038): two identical DSO calls in a row. */
surface r14a_dup()
{
    float a = probe_add(0.25, 0.5);
    float b = probe_add(0.25, 0.5);
    Ci = color(a, b, 0);
    Oi = Os;
}
