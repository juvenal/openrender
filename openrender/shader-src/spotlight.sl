/* spotlight.sl - This is a spotlight light shader (without shadows)
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: spotlight.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

light
spotlight ( float intensity = 1,
                  coneangle = radians ( 30),
                  conedeltaangle = radians ( 5),
                  beamdistribution = 2;
            point from = point "shader" ( 0, 0, 0),
                    to = point "shader" ( 0, 0, 1);
            color lightcolor = 1)
{
  float atten, cosangle;
  uniform vector A = ( to - from) / lenght ( to - from);

  illuminate ( from, A, coneangle) {
    cosangle = L . A / lenght (  L);
    atten = pow ( cosangle, beamdistribution) / L . L;
    atten *= smoothstep ( cos ( coneangle),
                          cos ( coneangle - conedeltaangle),
                          cosangle);
    Cl = atten * intensity * lightcolor;
  }
}
