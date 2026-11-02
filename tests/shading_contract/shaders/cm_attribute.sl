/* Spec 026 contract-test shader (tests/shading_contract). attribute(): a float ("ShadingRate" is a real name), a triple, a matrix, a string ("identifier:name" is a real name), a name that does not exist, and a stored value smaller than the destination ("cm:short": one float for a vector). */
surface cm_attribute()
{
    float f = -1;
    float f2 = -7;
    vector v = vector(-1, -2, -3);
    vector v2 = vector(-1, -2, -3);
    matrix m = 1;
    string s = "unset";
    float r1 = attribute("ShadingRate", f);
    float r2 = attribute("cm:triple", v);
    float r3 = attribute("cm:matrix", m);
    float r4 = attribute("identifier:name", s);
    float r5 = attribute("cm:missing", f2);
    float r6 = attribute("cm:short", v2);

    float untouched = (f2 == -7) ? 1 : 0;
    float strset = (s == "unset") ? 0 : 1;
    float tailkept = (ycomp(v2) == -2) ? 1 : 0;
    Ci = color(f, xcomp(v), comp(m, 1, 1));
    Oi = color(r1 + 2 * r2 + 4 * r3 + 8 * r4 + 16 * r5 + 32 * r6, strset + 2 * untouched + 4 * tailkept, xcomp(v2));
}
