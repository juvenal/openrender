/* Spec 056 contract-test shader (tests/shading_contract). A marker: wherever a step runs this shader the colour
 * is (0.9, 0.8, 0.7), which no canned answer ever has, so a test tells "ran" from "did not run". */
volume cv_counter()
{
    Ci = color(0.9, 0.8, 0.7);
    Oi = color(0.5);
}
