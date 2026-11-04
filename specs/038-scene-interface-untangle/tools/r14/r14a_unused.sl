/* R14a probe 2 (spec 038): a value-returning call whose result is never read. */
surface r14a_unused()
{
    float u = probe_add(0.25, 0.5);
    Ci = color(1, 0, 0);
    Oi = Os;
}
