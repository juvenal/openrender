/* GitHub #115 contract-test shader (tests/shading_contract). The second of two lights with different parameters of every type (see cm_ls_light_a). */
light cm_ls_light_b(float k = 2; vector v = vector(10, 20, 30); string s = "two"; matrix m = 3)
{
    illuminate(point "shader" (0, 0, 5)) {
        Cl = color(1, 1, 1);
    }
}
