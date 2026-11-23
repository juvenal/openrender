/* GitHub #115 contract-test shader (tests/shading_contract). The first of two lights with different parameters of every type, for lightsource() in an illuminance loop (RISpec 3.2, section 15.8). */
light cm_ls_light_a(float k = 1; vector v = vector(1, 2, 3); string s = "one"; matrix m = 2)
{
    illuminate(point "shader" (0, 0, 5)) {
        Cl = color(1, 1, 1);
    }
}
