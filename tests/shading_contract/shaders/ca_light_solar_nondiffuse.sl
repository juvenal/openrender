/* GitHub #197 contract shader: a directional (solar) light whose __nondiffuse (an OUTPUT variable) is set from `nondiffuse` per point:
 * diffuse() scales the light by (1 - __nondiffuse). The end of a solar block saves the output variables as the end of an illuminate block does. */
light ca_light_solar_nondiffuse(color c = 1; vector dir = vector (0, 0, -1); float nondiffuse = 1;
                                output varying float __nondiffuse = 1)
{
    __nondiffuse = nondiffuse;
    solar(dir, 0) {
        Cl = c;
    }
}
