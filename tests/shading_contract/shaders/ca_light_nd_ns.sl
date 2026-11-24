/* GitHub #197 contract shader: a light with BOTH output variables, set independently, so that a swapped saved-state index shows: diffuse() reads only
 * __nondiffuse and specular() only __nonspecular. */
light ca_light_nd_ns(color c = 1; point from = point (0, 0, 5); float nondiffuse = 0; float nonspecular = 0;
                     output varying float __nondiffuse = 0; output varying float __nonspecular = 0)
{
    __nondiffuse = nondiffuse;
    __nonspecular = nonspecular;
    illuminate(from) {
        Cl = c;
    }
}
