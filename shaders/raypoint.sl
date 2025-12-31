/**
 * raypoint(): Raytraced point light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
raypoint (float intensity = 1;
          color lightcolor = 1;
          float blur = 0;
          float numSamples = 1;
          point from = point "shader" (0,0,0);) {

    illuminate (from) {
        //Cl = shadow("raytrace", Ps, "blur", blur, "samples", numSamples)  * intensity * lightcolor / (L.L);
        //Cl = transmission(Ps, from, "blur", blur, "samples", numSamples) * intensity * lightcolor / (L.L);
        Cl = visibility(Ps, from) * intensity * lightcolor / (L.L);
    }
}
