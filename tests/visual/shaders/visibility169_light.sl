/* GitHub #169: a point light that scales its intensity by visibility(Ps, from), as the shipped area lights do per sample
 * (tests/visual/visibility-scenes.cmake, Visibility169_light_*).
 */
light visibility169_light(point from = point "shader" (0, 4, 0); float intensity = 1)
{
    illuminate(from) {
        Cl = intensity * visibility(Ps, from);
    }
}
