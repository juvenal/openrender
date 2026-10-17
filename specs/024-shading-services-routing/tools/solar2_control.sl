/* spec 024 research fixture (research R1): control for solar0_probe.sl.
 * The two-argument solar() form, whose JIT IR is expected to call
 * op_solar_begin.
 */
light solar2_control(float intensity = 1; color lightcolor = 1;
                     point from = point "shader" (0, 0, 0);
                     point to = point "shader" (0, 0, 1);)
{
    solar(to - from, 0) {
        Cl = intensity * lightcolor;
    }
}
