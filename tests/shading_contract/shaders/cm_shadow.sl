/* Spec 026 contract-test shader (tests/shading_contract). shadow("raytrace"), from a light shader (L is defined there; see validation.md D1). */
light cm_shadow(point from = point (0, 4, -2))
{
    illuminate(from) {
        float s = shadow("raytrace", Ps);
        Cl = color(1 - s, 1 - s, 1 - s);
    }
}
