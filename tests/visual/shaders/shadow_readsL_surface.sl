/* GitHub #236 (a): a surface that reads L and Cl outside any illuminance block. RISpec 3.2 table 12.1 defines both only inside
 * illuminance, so the value is not specified, but it must not be whatever an earlier point's lights left in the buffer: two scenes that
 * differ only in what ran before this point must give the same pixel. Ci is (|L|_1, |Cl|_1, 1); the blue 1 keeps the pixel non-zero. */
surface shadow_readsL_surface()
{
    float l = abs(comp(L, 0)) + abs(comp(L, 1)) + abs(comp(L, 2));
    float c = comp(Cl, 0) + comp(Cl, 1) + comp(Cl, 2);
    Ci = color(l, c, 1);
    Oi = 1;
}
