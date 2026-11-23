/* GitHub #115 contract-test shader (tests/shading_contract). A light with output parameters, which lightsource() reads from the light's saved state: a varying one that depends on the point and a uniform one. */
light cm_ls_light_out(output varying float vary = 0; output uniform float uni = 0)
{
    illuminate(point "shader" (0, 0, 5)) {
        vary = xcomp(P) + 5;
        uni = 9;
        Cl = color(1, 1, 1);
    }
}
