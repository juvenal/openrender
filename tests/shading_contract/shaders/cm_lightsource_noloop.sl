/* Spec 026 contract-test shader (tests/shading_contract). lightsource() outside an illuminance loop (FR-028: the interpreter dereferences a null light on master). */
surface cm_lightsource_noloop()
{
    float f = -1;
    float found = lightsource("cmfloat", f);
    Ci = color(f, found, 0);
    Oi = 1;
}
