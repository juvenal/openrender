/* background.sl - A simple background imager
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: background.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

imager
background ( color background = 1)
{
  Ci += ( 1 - alpha) * background;
  Oi = 1;
  alpha = 1;
}
