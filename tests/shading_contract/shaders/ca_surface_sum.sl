/* Spec 057 contract shader: Ci is the sum of Cl over every light that reaches the point (no cosine). */
surface ca_surface_sum()
{
    color c = 0;
    illuminance(P) {
        c += Cl;
    }
    Ci = c;
    Oi = 1;
}
