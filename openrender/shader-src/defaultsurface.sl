/* defaultsurface.sl - This is the default surface shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: defaultsurface.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

surface
defaultsurface ( float Kd = .8, Ka = .2)
{
  float diffuse;

  diffuse = I.N ;
  diffuse = ( diffuse * diffuse) / ( I.I * N.N) ;

  Ci = Os * Cs * ( Ka + Kd * diffuse ) ;
  Oi = Os;
}
