/**
 * Project: Pixie
 *
 * File: fbw.h
 *
 * Description:
 *   This file defines the Windows image displaying class.
 *   It is used to display the image on the screen.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef FBW_H
#define FBW_H

#include <windows.h>

#include "framebuffer.h"

/*
 * Class: CWinDisplay
 *
 * Description:
 *     The windows display class
 *
 * Notes:
 *
 */
class CWinDisplay : public CDisplay {
  public:
    CWinDisplay(const char *, const char *, int, int, int);
    ~CWinDisplay();

    // int ready();
    void main();
    void redraw();
    int data(int, int, int, int, float *);
    void finish();

    HANDLE thread;

  private:
    HINSTANCE hInst;    // current instance
    HWND hWnd;          // current window
    BITMAPINFO info;    // bitmap info
    RGBQUAD *bmiColors; // the colors
    unsigned int *imageData;
    int active;
    int willRedraw;
};

#endif
