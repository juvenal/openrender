/* Spec 057 contract shader: the same with a UNIFORM __nondiffuse (the light's nonDiffuseStep is 0), fixed at 1: diffuse() skips the light. */
light ca_light_nondiffuse_uniform(color c = 1; point from = point (0, 0, 5);
                                  output uniform float __nondiffuse = 1)
{
    illuminate(from) {
        Cl = c;
    }
}
