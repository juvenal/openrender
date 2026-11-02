/*
 * Spec 038 test shader (ShadeopAbi_Jit_Args): the shapes of argument a DSO call carries. A varying argument
 * with uniform arguments promoted beside it, a uniform call, a string argument next to a varying float, and a
 * call masked by an if (only the points where t > 0.5 may run it). The .rslo and the .slo render must agree
 * on every pixel, including the points the if masks.
 */
surface probe_args()
{
    float k = s;
    float a = jit_mix(k, 0.5, 0.25);
    float b = jit_mix(0.25, 0.5, 0.125);
    float c = jit_len("abc", k);
    float d = 0;
    if (t > 0.5) {
        d = jit_mix(t, k, 1.0);
    }
    Ci = color(a, b + c * 0.1, d);
    Oi = Os;
}
