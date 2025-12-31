/**
 * defaultsurface(): default surface shader
 *
 * From the "The RenderMan Companion"
 */

surface
easysurface (float Kd = 0.8,
                   Ka = 0.2,
                   falloff = 2.0) {

    float diffuse;

    diffuse = I.N / (I.I * N.N);
    diffuse = pow(diffuse, falloff);

    Ci = Cs * (Ka + Kd * diffuse);
}
