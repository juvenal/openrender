/* Spec 035 test shader (tests/closures), Phase 4, Closure_Bxdf_MessagePassing. A surface with a message parameter: a displacement shader of the
 * same primitive reads it with surface("x", v) (RISpec 3.2 section 15.8). Under a Bxdf this surface does not run; it still sits in the slot. */
surface cl_msg_surface(float x = 0.75)
{
    Ci = Cs * Os;
    Oi = Os;
}
