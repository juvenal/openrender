/* R14a probe 4 (spec 038): a DSO call in a loop body. */
surface r14a_loop()
{
    float s = 0;
    float i;
    for (i = 0; i < 3; i += 1) {
        s += probe_add(i, 0.5);
    }
    Ci = color(s, s, s);
    Oi = Os;
}
