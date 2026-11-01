/* Spec 026 contract-test shader (tests/shading_contract). opposite() against the real renderer, whose bound producer (cm_producer_*) holds float cmfloat, vector cmvec, matrix cmmatrix and string cmstring (FR-026): the matrix asked for as a vector, the string asked for as a float and the float asked for as a string cannot be copied safely. Each refused destination has a guard on both sides and stays untouched, each call returns 0. The float asked for as a float is copied and returns 1. Ci = (refused-call sum, the copied call's result, number of guards intact: 8); Oi = (destinations untouched: 7, the copied value). */
surface cm_mm_real_msg_opposite()
{
    vector gA = vector(7, 7, 7);
    vector dv = vector(-1, -2, -3);
    vector gB = vector(7, 7, 7);
    float fA = 7;
    float df = -4;
    float fB = 7;
    string ds = "unset";
    float dok = -5;

    float r1 = opposite("cmmatrix", dv);
    float r2 = opposite("cmstring", df);
    float r3 = opposite("cmfloat", ds);
    float r4 = opposite("cmfloat", dok);

    float guards = ((xcomp(gA) == 7) ? 1 : 0) + ((ycomp(gA) == 7) ? 1 : 0) + ((zcomp(gA) == 7) ? 1 : 0)
                 + ((xcomp(gB) == 7) ? 1 : 0) + ((ycomp(gB) == 7) ? 1 : 0) + ((zcomp(gB) == 7) ? 1 : 0)
                 + ((fA == 7) ? 1 : 0) + ((fB == 7) ? 1 : 0);
    float u1 = ((xcomp(dv) == -1) ? 1 : 0) + ((ycomp(dv) == -2) ? 1 : 0) + ((zcomp(dv) == -3) ? 1 : 0);
    u1 = (u1 == 3) ? 1 : 0;
    float u2 = (df == -4) ? 1 : 0;
    float u3 = (ds == "unset") ? 1 : 0;

    Ci = color(r1 + 2 * r2 + 4 * r3, r4, guards);
    Oi = color(u1 + 2 * u2 + 4 * u3, dok, 0);
}
