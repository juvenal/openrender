/**
 * metal(): metal surface shader
 *
 * openRender: RenderMan compliant renderer
 */

surface
metal (float Ka = 1,
             Ks = 1,
             roughness = 0.1) {

    normal Nf;

    Nf = faceforward(normalize(N), I) ;
    Oi = Os;
    Ci = Os * Cs * (Ka * ambient() + Ks * specular(Nf, normalize(-I), roughness));
}
