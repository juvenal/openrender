/* defaultlight.sl
 *
 * openRender is:
 * (C) Copyright 2002 Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 * All Rights Reserved.
 *
 * $Id: defaultlight.sl,v 1.1 2002/10/18 20:19:44 juvenal Exp $
 */

light
defaultlight()
{
  Cl = 0;
  L = - point "shader" ( 0, 0, 0);
}
