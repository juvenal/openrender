/**
 * ambientocclusion() light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
ambientocclusion (float numSamples = 16;) {

    vector Nf = faceforward(normalize(N), I);
    Cl = 1 - occlusion(P, Nf, numSamples);
    L = 0;
}

