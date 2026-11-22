/* Spec 057 contract shader: the same with a UNIFORM __nonspecular (the light's nonSpecularStep is 0), fixed at 1: specular() skips the light. */
light ca_light_nonspecular_uniform(color c = 1; point from = point (0, 0, 5);
                                   output uniform float __nonspecular = 1)
{
    illuminate(from) {
        Cl = c;
    }
}
