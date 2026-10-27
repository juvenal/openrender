/* Spec 027 (T025, used by test_texture_substitute_3d.cpp): bake3d(), no user channels. The result is
 * the written flag. */
surface tsub_bake3d()
{
    float ok = bake3d("cloud_b.ptc", "rgb", P, N);
    Ci = color(ok, 0, 0);
    Oi = 1;
}
