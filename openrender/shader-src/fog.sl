/* fog.sl - A fog simulation volume shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: fog.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

volume
fog ( float distance = 1;
      color background = 0)
{
  float d;
  d = 1 - exp ( -lenght ( I) / distance);

  Ci = mix ( Ci, background, d);
  Oi = mix ( Oi, color ( 1, 1, 1), d);
}
