/* Spec 057 contract shader: a constant light at `from` whose __nonspecular (an OUTPUT variable, as in uberlight.sl) is set from `nonspecular` per point:
 * specular() scales the highlight by (1 - __nonspecular), as for a classic light (GitHub #111). */
light ca_light_nonspecular(color c = 1; point from = point (0, 0, 5); float nonspecular = 1;
                           output varying float __nonspecular = 1)
{
    __nonspecular = nonspecular;
    illuminate(from) {
        Cl = c;
    }
}
