/* Spec 057 test shader (tests/arealight). Sums the light's Cl over the lights that reach the point (no cosine): a probe reads back what a light writes into Cl. */
surface al_surface_lightsum()
{
    color c = 0;
    illuminance(P) {
        c += Cl;
    }
    Ci = c;
    Oi = Os;
}
