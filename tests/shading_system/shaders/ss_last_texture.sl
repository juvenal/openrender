/* GitHub #268: the last instruction that takes operands is a stochastic one (texture), with a string constant ("blur")
 * in the first 8 bytes of the constant table. */
surface ss_last_texture(string tx = "")
{
    Ci = texture(tx, s, t, "blur", 0.1);
}
