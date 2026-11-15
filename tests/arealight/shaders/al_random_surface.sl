/* Spec 057 test shader (tests/arealight). A surface whose colour is three draws of random() and nothing else:
 * no lights, no texture, no illumination. Its output therefore depends only on the shader instance's own sample
 * stream key (research R11), so two renders of one scene differ exactly when the instance took another key. */
surface al_random_surface()
{
    Ci = color(random(), random(), random());
    Oi = color(1);
}
