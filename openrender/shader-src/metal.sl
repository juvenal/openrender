/* metal.sl - This is the surface shader simulating metal appearance
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: metal.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

surface
metal ( float Ka = 1, Ks = 1, roughness = .1)
{
  normal Nf = faceforward ( normalize ( N), I);
  vector V = -normalize ( I);

  Oi = Os;
  Ci = Os * Cs * ( Ka * ambient () + Ks * specular ( Nf, V, roughness));
}
