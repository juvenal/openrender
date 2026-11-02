/* Spec 026 contract-test shader (tests/shading_contract). A producer: a light with a parameter of each type. */
light cm_producer_light(float cmfloat = 2.5; vector cmvec = vector(0.25, 0.5, 0.75);
                       matrix cmmatrix = 1; string cmstring = "hello")
{
    illuminate(point "shader" (0, 0, 5)) {
        Cl = color(1, 1, 1);
    }
}
