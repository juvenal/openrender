/**
 * paintedplastic(): surface shader
 *
 * openRender: RenderMan compliant renderer
 */

surface
paintedplastic (float Ka = 1,
                      Kd = .5,
                      Ks = .5,
                      roughness = .1;
                color specularcolor = 1;
                string texturename = "") {

    Ci = Cs;
    if (texturename != "") {
        Ci *= color texture(texturename);
    }

    normal Nf = faceforward(normalize(N), I);
    Ci = Ci * (Ka * ambient() + Kd * diffuse(Nf)) +
         specularcolor * Ks * specular(Nf, -normalize(I), roughness);
    Oi = Os;
    Ci *= Oi;
}
