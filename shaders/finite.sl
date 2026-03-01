/**
 * finite(): 
 *
 *
 */

light 
finite (float intensity = 1;
        color lightcolor = 1;
        point location = point "shader" (0, 0, 0)) {

    L = vector (Ps - location);
    Cl = intensity * lightcolor;
}
