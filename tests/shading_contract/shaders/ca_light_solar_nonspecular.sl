/* GitHub #197 contract shader: a directional (solar) light whose __nonspecular (an OUTPUT variable) is set from `nonspecular` per point:
 * specular() scales the highlight by (1 - __nonspecular). */
light ca_light_solar_nonspecular(color c = 1; vector dir = vector (0, 0, -1); float nonspecular = 1;
                                 output varying float __nonspecular = 1)
{
    __nonspecular = nonspecular;
    solar(dir, 0) {
        Cl = c;
    }
}
