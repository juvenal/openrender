/* spec 024 FR-019: a light whose only construct is solar() with no
 * arguments. The compiler emits "solar <label>" for it.
 */
light solar0_light(float intensity = 1; color lightcolor = 1;)
{
    solar() {
        Cl = intensity * lightcolor;
    }
}
