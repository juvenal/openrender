/* constant.sl - This is a constant color surface shader
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: constant.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

surface
constant ()
{
  Oi = Os;
  Ci = Os * Cs;
}
