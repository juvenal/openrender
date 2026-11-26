/* GitHub #268 (the same loader): a string array parameter with an initializer. No float array comes before it, so the .rslo
 * loader's pass 2 had no float-array pointer to store the strings through. */
surface ss_string_array(string names[3] = {"a", "bb", "ccc"}; float Kd = 0.5)
{
    Ci = color(Kd);
}
