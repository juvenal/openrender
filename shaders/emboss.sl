/**
 *
 *
 *
 *
 *
 *
 */

 #include "includes/displacement.slh"

displacement
emboss(string texturename = "",
              dispspace = "shader";
        float Km = 1,
              truedisp = 1,
              sstart = 0,
              sscale = 1,
              tstart = 0,
              tscale = 1,
              blur = 0)
{
    if (texturename != "") {
        float ss = (s - sstart) / sscale;
        float tt = (t - tstart) / tscale;

        float amp = float texture(texturename[0], ss, tt, "blur", blur);

        N = Displace(normalize(N), dispspace, -Km * amp, truedisp);
    }
}