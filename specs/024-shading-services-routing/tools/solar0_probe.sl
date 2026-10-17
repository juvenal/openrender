/* spec 024 research fixture (research R1): the no-argument solar() form.
 * The compiler emits "solar <label>" for it. The research check compiles
 * this file to .rslo and with --jit, and looks for op_solar_begin in the
 * bitcode. solar2_control.sl is the two-argument control.
 */
light solar0_probe(float intensity = 1; color lightcolor = 1;)
{
    solar() {
        Cl = intensity * lightcolor;
    }
}
