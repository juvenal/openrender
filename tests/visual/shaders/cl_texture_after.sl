/* GitHub #268: the last instruction that takes operands is texture(), called with literal options. Its _after twin adds one
 * more statement and is the control: the two must render alike. Both define the surface cl_<call>, compiled into
 * different directories: a render depends on the shader's name, so the twins must share it. */
surface cl_texture()
{
    vector R = reflect(normalize(I), normalize(N));
    Ci = texture("texture-png-source.tex", s, t, "blur", 0.5);
    Ci = Ci * 1; /* exact: one more statement that takes operands */
}
