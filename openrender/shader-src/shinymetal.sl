/* shinymetal.sl - This is a surface shader simulating polished metal
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: shinymetal.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

surface
shinymetal ( float Ka = 1, Ks = 1, Kr = 1, roughness = .1;
             string texturename = "")
{
  normal Nf = faceforward ( normalize ( N), I);
  vector V = -normalize ( I);
  vector D = reflect ( I, normalize ( Nf));
  D = vtransform ( "current", "world", D);

  Oi = Os;
  Ci = Os * Cs * ( Ka * ambient () + Ks * specular ( Nf, V, roughness) +
                   Kr * color environment ( texturename, D));
}
