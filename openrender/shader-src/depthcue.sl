/* depthcue.sl - A depth cue volume shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: depthcue.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

volume
depthcue ( float mindistance = 0,
                 maxdistance = 1;
           color background = 0)
{
  float d;
  d = clamp ( ( depth ( P) - mindistance) / ( maxdistance - mindistance),
              0.0, 1.0);
  Ci = mix ( Ci, background, d);
  Oi = mix ( Oi, color ( 1, 1, 1), d);
}
