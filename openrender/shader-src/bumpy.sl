/* bumpy.sl - A bumpy displacement shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: bumpy.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

displacement
bumpy ( float Km = 1;
        string texturename = "")
{
  float amp = Km * float texture ( texturename, s, t);
  P += amp * normalize ( N);
  N = calculatenormal ( P);
}
