/**
 * spotlight(): spot light source shader
 *
 * openRender: RenderMan compliant renderer
 */

light
spotlight (float intensity = 1;
           color lightcolor = 1;
           point from = point "shader" (0, 0, 0),
                 to = point "shader" (0, 0, 1);
           float coneangle = radians(30),
                 conedeltaangle = radians(5),
                 beamdistribution = 2) {

	uniform vector axis = normalize(to - from);
	uniform float cosoutside = cos(coneangle);
	uniform float cosinside = cos(coneangle - conedeltaangle);

	illuminate (from, axis, coneangle) {
		float cosangle = (L.axis) / length(L);
		float atten = pow(cosangle, beamdistribution) / (L.L);
		atten *= smoothstep(cosoutside, cosinside, cosangle);
		Cl = atten * intensity * lightcolor;
	}
}
