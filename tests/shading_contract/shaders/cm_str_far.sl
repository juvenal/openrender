/* GitHub #176 contract-test shader (tests/shading_contract). Reads a string parameter of each producer into a string parameter of its own, so the test can read the copied pointer back from the parameter's slot without dereferencing it. */
surface cm_str_far(string s1 = "unset"; string s2 = "unset"; string s3 = "unset")
{
    float r1 = surface("cmstring", s1);
    float r2 = displacement("cmstring", s2);
    float r3 = atmosphere("cmstring", s3);
    Ci = color(r1, r2, r3);
    Oi = 1;
}
