//  openRender
//
//  utils.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Mon Oct 28 2002
//
//  Original Development:
//    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software, you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: utils.h,v 1.4 2004/01/07 11:33:19 juvenal Exp $
//

#ifndef UTILS_H
#define UTILS_H

namespace util {
    float max (float v1, float v2);
    float min (float v1, float v2);
    float maxOf (float v1, float v2, float v3);
    float minOf (float v1, float v2, float v3);
    float clampVal (float value, float min, float max);
} // namespace util

#endif // UTILS_H
