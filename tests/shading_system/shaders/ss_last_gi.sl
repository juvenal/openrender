/* GitHub #268 and #173: indirectdiffuse() last, with its sample count (12) the first constant. */
surface ss_last_gi()
{
    Ci = indirectdiffuse(P, N, 12);
}
