/* Spec 057 contract shader: the constant light with __nondiffuse = 1: diffuse() skips it, as for a classic light. */
light ca_light_nondiffuse(color c = 1; float __nondiffuse = 1)
{
    illuminate(P) {
        Cl = c;
    }
}
