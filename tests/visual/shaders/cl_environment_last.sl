/* GitHub #268: the last instruction that takes operands is environment(), called with literal options. Its _after twin adds one
 * more statement and is the control: the two must render alike. Both define the surface cl_<call>, compiled into
 * different directories: a render depends on the shader's name, so the twins must share it. */
surface cl_environment()
{
    vector R = reflect(normalize(I), normalize(N));
    Ci = environment("env-source.tex", R, "blur", 0.05);
}
