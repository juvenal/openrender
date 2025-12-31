/**
 * spotlight(): spot light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
spotlight (float intensity = 1;
           color lightcolor = 1;
           point from = point "shader" (0,0,0),
                 to = point "shader" (0,0,1);
           float coneangle = radians(30),
                 conedeltaangle = radians(5),
                 beamdistribution = 2) {

	uniform vector axis = normalize(to - from);

	illuminate (from, axis, coneangle) {
		float cosangle = (L.axis) / length(L);
		float atten = pow(cosangle, beamdistribution) / (L.L);
		atten *= smoothstep(cos(coneangle), cos(coneangle - conedeltaangle), cosangle);
		Cl = atten * intensity * lightcolor;
	}
}
