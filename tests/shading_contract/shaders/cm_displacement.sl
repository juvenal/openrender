/* Spec 026 contract-test shader (tests/shading_contract). displacement(): a parameter of each type (float, vector, matrix, string) and a name that does not exist. */
surface cm_displacement()
{
    float f = -1;
    float f2 = -7;
    vector v = vector(-1, -2, -3);
    matrix m = 1;
    string s = "unset";
    float r1 = displacement("cmfloat", f);
    float r2 = displacement("cmvec", v);
    float r3 = displacement("cmmatrix", m);
    float r4 = displacement("cmstring", s);
    float r5 = displacement("cmmissing", f2);

    Ci = color(f, xcomp(v), comp(m, 1, 1));
    Oi = color(r1 + 2 * r2 + 4 * r3 + 8 * r4 + 16 * r5, (s == "unset") ? 0 : 1, f2);
}
