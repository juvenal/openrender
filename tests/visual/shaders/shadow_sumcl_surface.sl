/* GitHub #236 (b): the receiver of shadow_prelight_light. Ci is the sum of Cl over the illuminance loop. */
surface shadow_sumcl_surface()
{
    color sum = 0;
    illuminance(P) { sum += Cl; }
    Ci = sum;
    Oi = 1;
}
