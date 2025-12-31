/**
 * paintedmatte(): surface shader
 *
 * openRender: RenderMan compliant renderer
 */

surface
paintedmatte (float Ka = 1,
                    Kd = 1;
              string texname = "") {

    normal Nf = faceforward(normalize(N), I);

    Ci = Cs;
    if (texname != "") {
        Ci *= texture(texname);
    }

    Ci = Ci * (Ka * ambient() + Kd * diffuse(Nf));
    Oi = Os;
    Ci *= Oi;
}
