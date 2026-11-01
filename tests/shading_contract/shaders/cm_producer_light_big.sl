/* Spec 026 contract-test shader (tests/shading_contract). A light with an array parameter larger than 16 floats and a small float parameter, for lightsource() under the JIT (D10: the JIT's float-only lightsource() must not overrun its scratch). */
light cm_producer_light_big(float k[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
                            float small = 7)
{
    illuminate(point "shader" (0, 0, 5)) {
        Cl = color(1, 1, 1);
    }
}
