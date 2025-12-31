/**
 * plastic(): surface shader
 *
 * openRender: RenderMan compliant renderer
 */

surface
plastic (float Ka = 1,
               Kd = 0.5,
               Ks = 0.5,
               roughness = 0.1;
         color specularcolor = 1) {

    normal Nf = faceforward (normalize(N), I);
    Ci = Cs * (Ka * ambient() + Kd * diffuse(Nf)) +
         specularcolor * Ks * specular(Nf, -normalize(I), roughness);
    Oi = Os;
    Ci *= Oi;
}
