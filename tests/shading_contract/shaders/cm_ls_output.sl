/* GitHub #115 contract-test shader (tests/shading_contract). lightsource() of a light's output parameters (read from the light's saved state), into a varying and into uniform destinations: a varying parameter into a uniform destination is a failure (the result is 0, the destination is unchanged).
 * Ci = (sum of vary - xcomp(P), the uniform destination of "vary", the uniform destination of "uni"); Oi = (the three results). */
surface cm_ls_output()
{
    float v = -1;
    uniform float u = -1;
    uniform float w = -1;
    float fv = 0, fu = 0, fw = 0, d = 0;
    illuminance(P, N, PI / 2) {
        fv += lightsource("vary", v);
        d += v - xcomp(P);
        fu += lightsource("vary", u);
        fw += lightsource("uni", w);
    }
    Ci = color(d, u, w);
    Oi = color(fv, fu, fw);
}
