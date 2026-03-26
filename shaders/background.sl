/*
 * background.sl -- imager shader to put a solid background color behind
 *                  an image.
 *
 * DESCRIPTION:
 *   Puts a solid background color in all the pixels which do not contain
 *   foreground objects.
 * 
 * PARAMETERS:
 *   bgcolor - the color of the background
 *
 * Entropy and BMRT are:
 * (c) Copyright 2001 Exluna, Inc.  Portions (c) 1990-2000 Larry Gritz.
 * All rights reserved.
 */

imager
background (color bgcolor = 1,
                  background = 1)
{
    Ci += (1-alpha) * (bgcolor * background);
    Oi = 1;
    alpha = 1;
}
