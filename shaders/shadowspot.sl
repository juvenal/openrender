/**
 * shadowspot(): spot light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
shadowspot (float intensity = 1;
            color lightcolor = 1;
            point from = point "shader" (0, 0, 0),
                  to = point "shader" (0 ,0, 1);
            string shadowname = "";
            float coneangle = radians(30),
                  conedeltaangle = radians(5),
                  beamdistribution = 2) {

    uniform vector axis = normalize(to - from);

    illuminate (from, axis, coneangle) {
        float cosangle = (L.axis) / length(L);
        float atten = pow (cosangle, beamdistribution) / (L.L);
        atten *= smoothstep(cos(coneangle), cos(coneangle - conedeltaangle), cosangle);

        if (shadowname != "") {
            atten *= (1 - shadow(shadowname, Ps));
        }

        Cl = atten * intensity * lightcolor;
    }
}
