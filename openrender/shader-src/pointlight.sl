/* pointlight.sl - This is the point light shader with falloff
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: pointlight.sl,v 1.2 2003/12/06 00:45:01 juvenal Exp $
 */

light
pointlight ( float intensity = 1;
             color lightcolor = 1;
             point from = point "shader" ( 0, 0, 0))
{
  illuminate ( from) {
    Cl = intensity * lightcolor / L.L;
  }
}
