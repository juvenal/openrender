/* Spec 026 contract-test shader (tests/shading_contract). The real renderer's answers to queries whose stored type cannot be copied into the destination (FR-026): rendererinfo("version") holds four floats and a vector is asked for, rendererinfo("renderer") holds a string and a float is asked for, option("sampling:sampler") holds a string and a float is asked for. Each refused destination has a guard on both sides and stays untouched, each call returns 0. Two queries that fit are copied: rendererinfo("renderer", string) and option("Format", vector). Ci = (refused-call sum, copied-call sum, number of guards intact: 8); Oi = (destinations untouched, x of Format, y of Format). */
surface cm_mm_real()
{
    vector gA = vector(7, 7, 7);
    vector ver = vector(-1, -2, -3);
    vector gB = vector(7, 7, 7);
    float fA = 7;
    float fr = -4;
    float fB = 7;
    string sr = "unset";
    vector fmt = vector(-1, -1, -1);

    float r1 = rendererinfo("version", ver);
    float r2 = rendererinfo("renderer", fr);
    float r3 = rendererinfo("renderer", sr);
    float r4 = option("Format", fmt);
    float r5 = option("sampling:sampler", fr);

    float guards = ((xcomp(gA) == 7) ? 1 : 0) + ((ycomp(gA) == 7) ? 1 : 0) + ((zcomp(gA) == 7) ? 1 : 0)
                 + ((xcomp(gB) == 7) ? 1 : 0) + ((ycomp(gB) == 7) ? 1 : 0) + ((zcomp(gB) == 7) ? 1 : 0)
                 + ((fA == 7) ? 1 : 0) + ((fB == 7) ? 1 : 0);
    float u1 = ((xcomp(ver) == -1) ? 1 : 0) + ((ycomp(ver) == -2) ? 1 : 0) + ((zcomp(ver) == -3) ? 1 : 0);
    u1 = (u1 == 3) ? 1 : 0;
    float u2 = (fr == -4) ? 1 : 0;
    float u3 = (sr == "unset") ? 0 : 1;

    Ci = color(r1 + 2 * r2 + 4 * r5, r3 + 2 * r4, guards);
    Oi = color(u1 + 2 * u2 + 4 * u3, xcomp(fmt), ycomp(fmt));
}
