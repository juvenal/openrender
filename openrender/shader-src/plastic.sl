/* plastic.sl - This is a surface shader simulating plastic
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: plastic.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

surface
plastic ( float Ka = 1, Kd = .5, Ks = .5, roughness = .1;
          color specularcolor = 1)
{
  normal Nf = faceforward ( normalize ( N), I);
  vector V = -normalize ( I);

  Oi = Os;
  Ci = Os * ( Cs * ( Ka * ambient () + Kd * diffuse ( Nf)) +
              specularcolor * Ks * specular ( Nf, V, roughness));
}
