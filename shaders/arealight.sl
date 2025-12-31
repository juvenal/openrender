/**
 * arealight(): light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
arealight (float intensity = 1;
           color lightcolor = 1) {

    N = normalize(N);

    illuminate (P, N, PI/2) {
        Cl = intensity * lightcolor * (N.normalize(L))/(L.L);
    }
}

