/* exposure.sl - An exposure imager shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: exposure.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

imager
exposure ( float gain = 1.0, gamma = 1.0, one = 255, min = 0, max = 255)
{
  Ci = pow ( gain * Ci, 1 / gamma);
  Ci = clamp ( round ( one * Ci), min, max);
  Oi = clamp ( round ( one * Oi), min, max);
}
