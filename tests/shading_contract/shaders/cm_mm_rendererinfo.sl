/* Spec 026 contract-test shader (tests/shading_contract). rendererinfo() against a double that answers TypeMismatch (FR-026): a stored matrix asked for as a vector, a stored string asked for as a float, a stored number asked for as a string. Each refused destination has a guard value on both sides of it; none of the three may be written, each call returns 0. The fourth query is a stored one-float value asked for as a vector: smaller than the destination, so it is copied (a partial write) and returns 1. Ci = (refused-call sum, the partial call's result, number of guards intact: 8); Oi = (destinations untouched, x of the partial copy, y of the partial copy kept). */
surface cm_mm_rendererinfo()
{
    vector gA = vector(7, 7, 7);
    vector dv = vector(-1, -2, -3);
    vector gB = vector(7, 7, 7);
    float fA = 7;
    float df = -4;
    float fB = 7;
    string ds = "unset";
    vector dshort = vector(-1, -2, -3);

    float r1 = rendererinfo("cm:mat", dv);
    float r2 = rendererinfo("cm:str", df);
    float r3 = rendererinfo("cm:num", ds);
    float r4 = rendererinfo("cm:short", dshort);

    /* no assignment under an if: GitHub #8 makes the JIT run it regardless */
    float guards = ((xcomp(gA) == 7) ? 1 : 0) + ((ycomp(gA) == 7) ? 1 : 0) + ((zcomp(gA) == 7) ? 1 : 0)
                 + ((xcomp(gB) == 7) ? 1 : 0) + ((ycomp(gB) == 7) ? 1 : 0) + ((zcomp(gB) == 7) ? 1 : 0)
                 + ((fA == 7) ? 1 : 0) + ((fB == 7) ? 1 : 0);

    float u1 = ((xcomp(dv) == -1) ? 1 : 0) + ((ycomp(dv) == -2) ? 1 : 0) + ((zcomp(dv) == -3) ? 1 : 0);
    u1 = (u1 == 3) ? 1 : 0;
    float u2 = (df == -4) ? 1 : 0;
    float u3 = (ds == "unset") ? 1 : 0;
    float tail = (ycomp(dshort) == -2) ? 1 : 0;

    Ci = color(r1 + 2 * r2 + 4 * r3, r4, guards);
    Oi = color(u1 + 2 * u2 + 4 * u3, xcomp(dshort), tail);
}
