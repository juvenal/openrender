/* GitHub #208: round() answers the integer closest to its argument (RISpec 3.2), halves away from zero, in both backends,
 * for the Round_* tests (tests/visual/round-scenes.cmake). The value comes from a parameter, so the compiler cannot fold it.
 *
 *   Ci = (round(v), 1 if 1 / round(v) is negative else 0, round(-v))
 *
 * 1 / -0 is -infinity and 1 / +0 is +infinity, so the second channel is 1 for a result of -0 and 0 for +0: it shows the sign of
 * a zero result. (atan(y, x) is not used for this: the JIT's two-argument atan ignores the sign of x.)
 */
surface round_probe(float v = 2.7)
{
    float r = round(v);
    float negative = (1 / r < 0) ? 1 : 0;
    Ci = color(r, negative, round(-v));
    Oi = 1;
}
