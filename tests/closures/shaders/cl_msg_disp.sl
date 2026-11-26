/* Spec 035 test shader (tests/closures), Phase 4, Closure_Bxdf_MessagePassing. A displacement shader that reads a message of the Surface
 * attached to its primitive: r_msg is the value when the call finds the parameter, -1 when it does not. It moves nothing. */
displacement cl_msg_disp(output varying float r_msg = 0;)
{
    float v = 0;
    if (surface("x", v) == 1)
        r_msg = v;
    else
        r_msg = -1;
}
