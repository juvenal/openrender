/* Spec 029 contract-test shader (tests/shading_contract). A light with one illuminate1 construct: the construct reaches the illumination hook. */
light cm_hook_illuminate1(point from = point(0.5, 1.5, 2.5); vector axis = vector(0, 0, 1); float angle = 0.5;)
{
    illuminate(from) { Cl = color(1, 1, 1); }
}
