/**
 * matte(): matte surface shader
 *
 * openRender: RenderMan compliant renderer
 */

surface
matte (float Ka = 1,
             Kd = 1) {

    normal Nf = faceforward (normalize(N), I);
    Ci = Cs * (Ka * ambient() + Kd * diffuse(Nf));
    Oi = Os;
    Ci *= Oi;
}
