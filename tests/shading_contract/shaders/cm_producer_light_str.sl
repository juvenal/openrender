/* GitHub #176 contract-test shader (tests/shading_contract). A light with a mutable string parameter: its value is kept in the light's saved state, which lightsource() reads. */
light cm_producer_light_str(output uniform string cmstring = "hello")
{
    illuminate(point "shader" (0, 0, 5)) {
        Cl = color(1, 1, 1);
    }
}
