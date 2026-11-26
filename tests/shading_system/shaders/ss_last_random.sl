/* GitHub #268: random() with nothing but its call-site ordinal as the last operand-taking instruction. */
surface ss_last_random()
{
    Ci = random();
}
