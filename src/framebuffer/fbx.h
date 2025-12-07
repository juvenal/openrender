/*
 * Project: Pixie
 *
 * File: fbx.h
 *
 * Description:
 *   This file defines the X Window image displaying class.
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

#ifndef FBX_H
#define FBX_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include "framebuffer.h"

/*
 * Class: CXDisplay
 *
 * Description:
 *     The windows display class
 *
 * Notes:
 *
 */
class CXDisplay : public CDisplay {
  public:
    CXDisplay(const char *, const char *, int, int, int);
    ~CXDisplay();

    void main();
    int data(int, int, int, int, float *);
    void finish();

  private:
    typedef void (CXDisplay::*dataHandlerFn)(int, int, int, int, float *);

    void handleData_rgb15(int, int, int, int, float *);
    void handleData_rgb15_rev(int, int, int, int, float *);
    void handleData_bgr15(int, int, int, int, float *);
    void handleData_bgr15_rev(int, int, int, int, float *);

    void handleData_rgb16(int, int, int, int, float *);
    void handleData_rgb16_rev(int, int, int, int, float *);
    void handleData_bgr16(int, int, int, int, float *);
    void handleData_bgr16_rev(int, int, int, int, float *);

    void handleData_rgba32(int, int, int, int, float *);
    void handleData_argb32(int, int, int, int, float *);
    void handleData_bgra32(int, int, int, int, float *);
    void handleData_abgr32(int, int, int, int, float *);

    dataHandlerFn dataHandler;
    pthread_t thread;
    void *imageData;
    int imageDepth;
    int scanWidth;
    int windowUp;
    int windowDown;
    Window xcanvas;
    Display *display;
    int screen;
    GC image_gc;
    XImage *xim;
    Atom WM_DELETE_WINDOW;
    Atom WM_PROTOCOLS;
    char *displayName;
};

#endif
