/* R14c probe (spec 038): the varying answer. A varying argument (the texture coordinate s), and the
 * same call under an if. Compiled by r14a.sh, whose output shows which operands are varying. */
surface r14c_vary()
{
    float k = s;
    float v = probe_add(k, 0.5);
    float w = 0;
    if (t > 0.5) {
        w = probe_add(t, 0.25);
    }
    Ci = color(v, w, 0);
    Oi = Os;
}
