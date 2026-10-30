/* GitHub #111: a distant light with a uniform __nonspecular, for the
 * ShadingParity_JitSpecular_* tests (tests/visual/CMakeLists.txt).
 */
light jit_nonspecular_uniform_light(float intensity = 1;
                                    point from = point "shader" (0, 0, 0);
                                    point to = point "shader" (0, 0, 1);
                                    output uniform float __nonspecular = 1)
{
    solar (to - from, 0) {
        Cl = intensity;
    }
}
