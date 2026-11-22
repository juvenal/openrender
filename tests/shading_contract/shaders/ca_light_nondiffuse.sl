/* Spec 057 contract shader: a constant light at `from` whose __nondiffuse (an OUTPUT variable, as in uberlight.sl: the engine reads it from the light's saved
 * state, not from an input parameter) is set from `nondiffuse` per point: diffuse() scales the light by (1 - __nondiffuse), as for a classic light. */
light ca_light_nondiffuse(color c = 1; point from = point (0, 0, 5); float nondiffuse = 1;
                          output varying float __nondiffuse = 1)
{
    __nondiffuse = nondiffuse;
    illuminate(from) {
        Cl = c;
    }
}
