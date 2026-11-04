/* R14a probe 3 (spec 038): a call followed by a read of its destination, where the destination
 * held a known constant before the call (a pass that propagates the constant across the call
 * would read 2.0 instead of the operand's 0.75). */
surface r14a_readback()
{
    float v = 2.0;
    v = probe_add(0.25, 0.5);
    float w = v + 1.0;
    float x = v * 4.0;
    Ci = color(w, x, v);
    Oi = Os;
}
