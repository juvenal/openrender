/**
 * mtorDirectionalLight(): directional light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
mtorDirectionalLight (float intensity = 1;
                      color lightcolor = (1, 1, 1);
                      vector direction = (0, 0, 1);
                      string shadowname = "";
                      color shadowcolor = (0, 0, 0);
                      float decayRate = 0) {

    if (shadowname == "") {
        solar (direction, 0) {
            Cl = intensity * lightcolor;
        }
    }
    else {
        solar (direction, 0) {
            Cl = mix(shadowcolor, intensity * lightcolor, shadow(shadowname, Ps));
        }
    }
}
