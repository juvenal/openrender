/* GitHub #203: the message and query builtins that answer a value with no variable, read into a VARYING destination, for the
 * ShadingParity_JitParamFinish_* and JitParamFinish_* tests (tests/visual/CMakeLists.txt). Every point must receive the value:
 * under the interpreter and the LLVM JIT alike.
 *
 *   mode  0 incident("testVolParam"), 1 opposite("testVolParam"), 2 attribute("ShadingRate"), 3 option("sampling:seed")
 *
 * The destination starts as 5 + 10 * s, a different value at every point, so a point the call did not write cannot be
 * mistaken for the answer. Ci = (destination, found, 0).
 */
surface jit_param_finish_probe(float mode = 0)
{
    float f = 5 + 10 * s;
    float found = 0;
    if (mode == 0)
        found = incident("testVolParam", f);
    else if (mode == 1)
        found = opposite("testVolParam", f);
    else if (mode == 2)
        found = attribute("ShadingRate", f);
    else
        found = option("sampling:seed", f);
    Ci = color(f, found, 0);
    Oi = 1;
}
