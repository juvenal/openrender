/**
 * shadowarea(): Shadow area light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
shadowarea (float intensity = 1;
            color lightcolor = 1) {

    N = normalize(N);

    illuminate (P, N, PI/2) {
        Cl = visibility(Ps, P) * intensity * lightcolor * (N.normalize(L)) / (L.L);
    }
}
