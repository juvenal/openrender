/* matte.sl - This is a matte surface shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: matte.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

surface
matte ( float Ka = 1, Kd = 1)
{
  normal Nf = faceforward ( normalize ( N), I);

  Oi = Os;
  Ci = Os * Cs * ( Ka * ambient () + Kd * diffuse ( Nf));
}
