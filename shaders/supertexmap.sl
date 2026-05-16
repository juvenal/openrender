/**
 *
 *
 *
 *
 * openRender: RenderMan compliant renderer
 */

#include "projections.slh"
#include "displacement.slh"
#include "texture_color.slh"

surface
supertexmap (float  Ka = 1, Kd = 0.5, Ks = 0.5, roughness = 0.1;
             color  specularcolor = 1;
             string Csmapname = "", Csproj = "st", Csspace = "shader";
             float  Csmx[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
             string Osmapname = "", Osproj = "st", Osspace = "shader";
             float  Osmx[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
             string Ksmapname = "", Ksproj = "st", Ksspace = "shader";
             float  Ksmx[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
             string dispmapname = "", dispproj = "st", dispspace = "shader";
             float  dispmx[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
             float  truedisp = 1)
{
    color Ct = Cs, Ot = Os;
    float ks = Ks, disp = 0;

    if (Csmapname != "") {
        Ct = ApplyColorTextureOver(Ct, Csmapname, Csproj, P,
                                   Csspace, array_to_mx(Csmx), 0);
    }
    if (Osmapname != "") {
        Ot = ApplyColorTextureOver(Ot, Osmapname, Osproj, P,
                                   Osspace, array_to_mx(Osmx), 0);
    }
    if (Ksmapname != "") {
        ks = ApplyFloatTextureOver(Ks, Ksmapname, Ksproj, P,
                                  Ksspace, array_to_mx(Ksmx), 0);
    }
    if (dispmapname != "") {
        disp = ApplyFloatTextureOver(disp, dispmapname, dispproj, P,
                                     dispspace, array_to_mx(dispmx), 0);
        N = Displace(normalize(N), dispspace, disp, truedisp);
    }

    normal Nf = faceforward(normalize(N), I);
    Ci = Ct * (Ka * ambient() + Kd * diffuse(Nf)) +
              specularcolor * ks *specular(Nf, -normalize(I), roughness);
    Oi = Ot;
    Ci *= Oi;
}
