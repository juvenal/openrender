/* Spec 057 contract shader: the constant light in the category "ca_cat" (the __category of a bound light acts as a classic light's). */
light ca_light_cat(color c = 1; string __category = "ca_cat")
{
    illuminate(P) {
        Cl = c;
    }
}
