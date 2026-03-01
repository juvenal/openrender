/**
 * finite(): Finite light source from RenderMan companion.
 *
 * Updated to strictly follow the latest RenderMan Spec 3.2.
 */

light 
finite (float intensity = 1;
        color lightcolor = 1;
        point location = point "shader" (0, 0, 0)) {

    illuminate (location) {
        Cl = intensity * lightcolor;
    }
}
