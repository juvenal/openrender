/* paintedplastic.sl - This is a painted plastic surface shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: paintedplastic.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

surface
paintedplastic ( float Ka = 1, Kd = .5, Ks = .5, roughness = .1;
                 color specularcolor = 1;
                 string texturename = "")
{
  normal Nf = faceforward ( normalize ( N), I);
  vector V = -normalize ( I);

  Oi = Os;
  Ci = Os * ( Cs * color texture ( texturename) *
            ( Ka * ambient () + Kd * diffuse ( Nf)) +
            specularcolor * Ks * specular ( Nf, V, roughness));
}
