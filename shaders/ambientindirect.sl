/**
 * ambientindirect(): light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
ambientindirect (float numSamples = 16,
                       intensity = 0.2) {

    vector Nf = faceforward(normalize(N), I);
    Cl = indirectdiffuse(P, Nf, numSamples);
    L = 0;
}

