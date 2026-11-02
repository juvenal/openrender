/* Spec 026 contract-test shader (tests/shading_contract). A producer: a surface with a parameter of each type, read by the message-passing builtins. */
surface cm_producer_surface(float cmfloat = 2.5; vector cmvec = vector(0.25, 0.5, 0.75);
                            matrix cmmatrix = 1; string cmstring = "hello")
{
    Ci = color(cmfloat, 0, 0);
    Oi = 1;
}
