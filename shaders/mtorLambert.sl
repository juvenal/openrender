/**
 * mtorLambert(): Lambert surface shader
 *
 * openRender: RenderMan compliant renderer
 */

surface
mtorLambert(float refractiveIndex = 1,
                  diffuseCoeff = 1;
            color ambientColor = (0, 0, 0),
                  incandescence = (0, 0, 0);
            float translucenceCoeff = 0,
                  glowIntensity = 0) {

    normal Nf = faceforward(normalize(N), I);
    Ci = Cs * (diffuseCoeff * diffuse(Nf)) + incandescence + ambientColor * ambient();
    Oi = Os;
    Ci *= Oi;
}
