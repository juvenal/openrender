/* Spec 035 test shader (tests/closures), Phase 4. A surface whose colour is three draws of random() and nothing else: no lights, no
 * texture, no illumination. Its output depends only on the shader instance's own sample-stream key (the handle key), so two renders of
 * one scene differ exactly when the instance took another key (the lesson of tests/arealight's LightKey_NoLeakAcrossFailedLight). */
surface cl_random()
{
    Ci = color(random(), random(), random());
    Oi = color(1);
}
