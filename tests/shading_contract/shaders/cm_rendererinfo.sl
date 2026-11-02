/* Spec 026 contract-test shader (tests/shading_contract). rendererinfo(): a float, a triple, a matrix, a string ("renderer" is a real name), a name that does not exist, and a stored value smaller than the destination. */
surface cm_rendererinfo()
{
    float f = -1;
    float f2 = -7;
    vector v = vector(-1, -2, -3);
    vector v2 = vector(-1, -2, -3);
    matrix m = 1;
    string s = "unset";
    float r1 = rendererinfo("cm:float", f);
    float r2 = rendererinfo("cm:triple", v);
    float r3 = rendererinfo("cm:matrix", m);
    float r4 = rendererinfo("renderer", s);
    float r5 = rendererinfo("cm:missing", f2);
    float r6 = rendererinfo("cm:short", v2);

    float untouched = (f2 == -7) ? 1 : 0;
    float strset = (s == "unset") ? 0 : 1;
    float tailkept = (ycomp(v2) == -2) ? 1 : 0;
    Ci = color(f, xcomp(v), comp(m, 1, 1));
    Oi = color(r1 + 2 * r2 + 4 * r3 + 8 * r4 + 16 * r5 + 32 * r6, strset + 2 * untouched + 4 * tailkept, xcomp(v2));
}
