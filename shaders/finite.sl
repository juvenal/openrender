/**
 * finite(): 
 *
 *
 */

light 
finite (float intensity = 1;
        color lightcolor = 1;
        vector location = (0, 0, 0)) {

    L = Ps - location;
    Cl = intensity * lightcolor;
}

