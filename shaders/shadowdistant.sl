/**
 * shadowdistant(): Raytraced directional light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
shadowdistant (float intensity = 1;
               color lightcolor = 1;
               point from = point "shader" (0,0,0),
                     to = point "shader" (0,0,1);
            string shadowname = "") {

    vector dir = to - from;

    solar (dir, 0) {
        color vis;
        if (shadowname == "") {
            vis	= color "rgb" (1, 1, 1);
        }
 		else {
            vis = (1 - shadow(shadowname, Ps));
        }

        Cl = vis * intensity * lightcolor;
    }
}
