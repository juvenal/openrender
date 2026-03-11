/**
 * material_thin_plastic():
 *     Compute the color of a surface using a simple, thin, plastic-like BRDF.
 *     We call it _thin_ because it  includes a transmission component to allow
 *     light from the _back_ of the surface to affect the appearance.
 *     Typical values are Ka = 1, Kd = 0.8, Kt = 0.2, Ks = 0.5, roughness = 0.1.
 *
 * openRender: RenderMan compliant renderer
 */

color MaterialThinPlastic(normal Nf;
                          vector V;
                          color basecolor;
                          float Ka, Kd, Kt, Ks, roughness) {

    return basecolor * (Ka * ambient() + Kd * diffuse(Nf) + Kt * diffuse(-Nf))
           + Ks * specular(Nf, V, roughness);
}
