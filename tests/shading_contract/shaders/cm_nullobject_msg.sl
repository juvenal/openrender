/* Spec 026 contract-test shader (tests/shading_contract). Run with no current object, as the imager does (FR-027, F2): surface(), displacement(), atmosphere(), incident() and opposite() have nothing to read and answer 0 without crashing, leaving their destinations untouched; shadername("surface") returns the empty string. Ci = (sum of the five results: 0, the name is empty: 1, 0); Oi = (the five destinations untouched: 31, 0, 0). */
surface cm_nullobject_msg()
{
    float f1 = -1;
    float f2 = -2;
    float f3 = -3;
    float f4 = -4;
    float f5 = -5;
    float r1 = surface("cmfloat", f1);
    float r2 = displacement("cmfloat", f2);
    float r3 = atmosphere("cmfloat", f3);
    float r4 = incident("cmfloat", f4);
    float r5 = opposite("cmfloat", f5);
    string n = shadername("surface");

    float untouched = ((f1 == -1) ? 1 : 0) + ((f2 == -2) ? 2 : 0) + ((f3 == -3) ? 4 : 0) + ((f4 == -4) ? 8 : 0) + ((f5 == -5) ? 16 : 0);
    Ci = color(r1 + r2 + r3 + r4 + r5, (n == "") ? 1 : 0, 0);
    Oi = color(untouched, 0, 0);
}
