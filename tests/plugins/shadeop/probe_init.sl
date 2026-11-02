/*
 * Spec 038 test shader (ShadeopAbi_Jit_Init, decision Q9): a parameter default that calls a DSO operand. The
 * default is evaluated by the init section at bind time, on thread index 0, not by the shading threads.
 */
surface probe_init(float base = 0.5; float scale = jit_mix(0.25, 0.5, 0.125))
{
    Ci = color(base, scale, scale * base);
    Oi = Os;
}
