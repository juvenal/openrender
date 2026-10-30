/* GitHub #111: a distant light with a varying __nonspecular, set from the
 * nonspecular parameter, for the ShadingParity_JitSpecular_* tests
 * (tests/visual/CMakeLists.txt).
 */
light jit_nonspecular_light(float intensity = 1;
                            point from = point "shader" (0, 0, 0);
                            point to = point "shader" (0, 0, 1);
                            float nonspecular = 1;
                            output varying float __nonspecular = 1)
{
    __nonspecular = nonspecular;
    solar (to - from, 0) {
        Cl = intensity;
    }
}
