/* ripple.sl - A ripple displacement shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: ripple.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

displacement
ripple ( float amplitude = 1.0, wavelenght = .25)
{
  P += N * amplitude * sin ( 2 * PI * ( s / wavelenght));
  N = calculatenormal ( P);
}
