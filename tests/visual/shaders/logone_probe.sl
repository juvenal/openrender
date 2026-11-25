/* GitHub #229: the one-argument log(x) answers log(max(x, 1e-6)) in both backends (the interpreter's clamp, scriptFunctions.h),
 * for the LogOne_* tests (tests/visual/atan2-scenes.cmake). The value comes from a parameter, so the compiler cannot fold it.
 *
 *   Ci = (log(x), log(-x), log(x + x))
 */
surface logone_probe(float x = 1)
{
    Ci = color(log(x), log(-x), log(x + x));
    Oi = 1;
}
