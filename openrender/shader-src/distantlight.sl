/* distantlight.sl - This is a distant light shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: distantlight.sl,v 1.2 2003/12/06 00:45:01 juvenal Exp $
 */

light
distantlight ( float intensity = 1;
               color lightcolor = 1;
               point from = point "shader" ( 0, 0, 0),
                       to = point "shader" ( 0, 0, 1))
{
  solar ( to - from, 0.0) {
    Cl = intensity * lightcolor;
  }
}
