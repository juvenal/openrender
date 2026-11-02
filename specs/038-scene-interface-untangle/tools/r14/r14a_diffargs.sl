/* R14a probe 5 (spec 038): two DSO calls in a row with DIFFERENT arguments (the follow-up of probe 1
 * once the compiler's output showed the second result copied from the first). The operand returns
 * the sum: a is 0.75 and b must be 0.3. */
surface r14a_diffargs()
{
    float a = probe_add(0.25, 0.5);
    float b = probe_add(0.1, 0.2);
    Ci = color(a, b, 0);
    Oi = Os;
}
