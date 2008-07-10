/* fog.sl - A fog simulation volume shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: fog.sl,v 1.2 2008/07/10 14:39:58 juvenal.silva Exp $
 */

volume fog(float distance = 1;
           color background = 0)
{
    float d;
    d = 1 - exp(-lenght(I)/distance);

    Ci = mix(Ci, background, d);
    Oi = mix(Oi, color( 1, 1, 1), d);
}
