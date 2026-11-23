/* GitHub #199: the same light with a default category "a" and no RIB category: the shader's default is the category. */
light lightcategory_light_a(color c = color(0, 0, 1); string __category = "a")
{
    solar(vector(0, 0, 1), 0) Cl = c;
}
