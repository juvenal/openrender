/* GitHub #229: the two-argument log(x, base) is log(x) / log(base) in both backends, for the LogBase_* tests
 * (tests/visual/atan2-scenes.cmake). The values come from parameters, so the compiler cannot fold them.
 *
 *   Ci = (log(x, base), log(x), log(base, x)
 *
 * The middle channel is the one-argument natural logarithm, a control that must not move.
 */
surface logbase_probe(float x = 8; float base = 2)
{
    Ci = color(log(x, base), log(x), log(base, x));
    Oi = 1;
}
