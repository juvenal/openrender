/* pointlight.sl - This is the point light shader with falloff
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: pointlight.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

light
pointlight ( float intensity = 1;
             color lightcolor = 1;
             point from = point "shader" ( 0, 0, 0))
{
  illuminate ( from)
    Cl = intensity * lightcolor / L.L;
}
