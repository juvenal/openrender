/* ambientlight.sl - This is a simple ambient light
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: ambientlight.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

light
ambientlight ( float intensity = 1;
               color lightcolor = 1)
{
  Cl = intensity * lightcolor;
}
