/* GitHub #203 contract-test shader (tests/shading_contract). The five message builtins read a value the services answer
 * without a variable into a VARYING destination: every point must receive it (the interpreter's PARAMETEREXPR_PRE does,
 * and so must jitParameterFinish). Each destination starts as a different value per point (-1 + 0 * s is varying by
 * construction, so the JIT keeps the destination varying). */
surface cm_novar_varying()
{
    float a = -1 + 0 * s;
    float b = -1 + 0 * s;
    float c = -1 + 0 * s;
    float d = -1 + 0 * s;
    float e = -1 + 0 * s;
    float r1 = surface("nvsurface", a);
    float r2 = displacement("nvdisplacement", b);
    float r3 = atmosphere("nvatmosphere", c);
    float r4 = incident("nvincident", d);
    float r5 = opposite("nvopposite", e);

    Ci = color(a, b, c);
    Oi = color(d, e, r1 + 2 * r2 + 4 * r3 + 8 * r4 + 16 * r5);
}
