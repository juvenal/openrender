/**
 * glass(): glass surface shader
 *
 * openRender: RenderMan compliant renderer
 */

surface
glass (float eta = 1.5,
	         Ka = 1,
	         Kd = 1,
	         Ks = 1,
	         roughness = 0.5) {

    vector R, T, In;
    float Kr, Kt;
    normal Nf;

    // Normalize some vectors
    Nf = faceforward(normalize(N), I);
    In = normalize(I);
    Ci = 0;

    // Compute the reflection and refraction
    fresnel(In, Nf, (In.N < 0 ? 1/eta : eta), Kr, Kt, R, T);

    if (I.N < 0) {
        Ci = Ks*specular(Nf, -In, roughness);
    }

    // Trace the reflection / refraction
    if (Kr > 0.01) {
        Ci += Kr * trace(P, R);
    }
    if (Kt > 0.01) {
        Ci += Cs * Kt * trace(P, T);
    }

    Ci *= Os;
    Oi = Os;
}
