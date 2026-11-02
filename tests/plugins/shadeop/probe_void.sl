/*
 * Spec 038 test shader (ShadeopAbi_Jit_Void): a void operand. The operand writes the value it receives to the
 * call log: once per point for a varying argument, once per call for a uniform one, and only where t > 0.5
 * inside the if. The records of the .rslo render and of the .slo render must be the same multiset.
 */
surface probe_void()
{
    float k = s;
    jit_record(k);
    jit_record(0.125);
    if (t > 0.5) {
        jit_record(t);
    }
    Ci = color(k, k, k);
    Oi = Os;
}
