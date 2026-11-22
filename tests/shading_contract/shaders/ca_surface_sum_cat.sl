/* Spec 057 contract shader: Ci is the sum of Cl over the lights of category `cat`. */
surface ca_surface_sum_cat(string cat = "ca_cat")
{
    color c = 0;
    illuminance(cat, P) {
        c += Cl;
    }
    Ci = c;
    Oi = 1;
}
