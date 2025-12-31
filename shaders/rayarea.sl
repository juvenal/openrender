/*	area(): light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
rayarea (float intensity = 1;
         color lightcolor = 1) {

    N = normalize(N);

    illuminate (P, N, PI/2) {
        Cl = visibility(P, Ps) * intensity * lightcolor * (N.normalize(L)) / (L.L);
    }
}
