/* Spec 027 (T025, used by test_texture_substitute_3d.cpp): texture3d(), no user channels (the JIT binds
 * none, so both backends make the same forwarder calls). The result is the found flag. */
surface tsub_texture3d()
{
    float found = texture3d("cloud_a.ptc", P, N);
    Ci = color(found, 0, 0);
    Oi = 1;
}
