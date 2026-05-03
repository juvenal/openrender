/**
 * Project: openRender
 *
 * File: fbx.cpp
 *
 * Description:
 *   This file implements the X window that display the image.
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

#include "fbx.h"
#include "common/global.h"
#include "framebuffer.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/wait.h>

#define color_15_bgr(r, g, b, a) ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
#define color_15_bgr_rev(r, g, b, a) ((g >> 3) << 13) | ((b >> 3) << 8) | ((r >> 3) << 3) | (g >> 5)
#define color_15_rgb(r, g, b, a) ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)
#define color_15_rgb_rev(r, g, b, a) ((g >> 3) << 13) | ((r >> 3) << 8) | ((b >> 3) << 3) | (g >> 5)

#define color_16_bgr(r, g, b, a) ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
#define color_16_bgr_rev(r, g, b, a) ((g >> 2) << 13) | ((b >> 3) << 8) | ((r >> 3) << 3) | (g >> 5)
#define color_16_rgb(r, g, b, a) ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3)
#define color_16_rgb_rev(r, g, b, a) ((g >> 2) << 13) | ((r >> 3) << 8) | ((b >> 3) << 3) | (g >> 5)

#define color_bgra(r, g, b, a) ((a << 24) | (r << 16) | (g << 8) | b)
#define color_abgr(r, g, b, a) ((r << 24) | (g << 16) | (b << 8) | a)
#define color_argb(r, g, b, a) ((b << 24) | (g << 16) | (r << 8) | a)
#define color_rgba(r, g, b, a) ((a << 24) | (b << 16) | (g << 8) | r)

#define get_pix_rgba(col)                 \
    float old_r = ((col) & 0xFF);         \
    float old_g = (((col) >> 8) & 0xFF);  \
    float old_b = (((col) >> 16) & 0xFF); \
    float old_a = (((col) >> 24) & 0xFF);

#define get_pix_argb(col)                 \
    float old_a = ((col) & 0xFF);         \
    float old_r = (((col) >> 8) & 0xFF);  \
    float old_g = (((col) >> 16) & 0xFF); \
    float old_b = (((col) >> 24) & 0xFF);

#define get_pix_bgra(col)                 \
    float old_b = ((col) & 0xFF);         \
    float old_g = (((col) >> 8) & 0xFF);  \
    float old_r = (((col) >> 16) & 0xFF); \
    float old_a = (((col) >> 24) & 0xFF);

#define get_pix_abgr(col)                 \
    float old_a = ((col) & 0xFF);         \
    float old_b = (((col) >> 8) & 0xFF);  \
    float old_g = (((col) >> 16) & 0xFF); \
    float old_r = (((col) >> 24) & 0xFF);

/*
 * Function: displayThread
 *
 * Description:
 *     Starts the display thread
 *
 * Return:
 *     On Success: The handle of the image
 *     On Failure: NULL pointer
 *
 * Notes:
 *
 */
void *displayThread(void *w) {
    CXDisplay *cDisplay = (CXDisplay *)w;

    // The thread main loop
    cDisplay->main();

    return NULL;
}

/*
 * Class: CXDisplay
 *
 * Method: CXDisplay (Constructor)
 *
 * Description:
 *     Initialize new instances of this class
 *
 * Return:
 *
 * Notes:
 *
 */
CXDisplay::CXDisplay(const char *name, const char *samples, int width, int height, int numSamples)
    : CDisplay(name, samples, width, height, numSamples) {
    int x, y;
    unsigned int *dest;
    unsigned short *dests;
    unsigned short byteOrder = 0x0100;
    int flipByteOrder;
    Visual *vis;

    XInitThreads();
    display = XOpenDisplay(NULL);

    if (display == NULL) {
        failure = TRUE;
    }
    else {
        WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
        WM_PROTOCOLS     = XInternAtom(display, "WM_PROTOCOLS",     False);
        _NET_WM_NAME     = XInternAtom(display, "_NET_WM_NAME",     False);
        _NET_WM_PID      = XInternAtom(display, "_NET_WM_PID",      False);
        UTF8_STRING      = XInternAtom(display, "UTF8_STRING",      False);

        screen = DefaultScreen(display);
        imageDepth = DefaultDepth(display, screen);
        flipByteOrder = ImageByteOrder(display);
        vis = DefaultVisual(display, screen);

        flipByteOrder = (*((unsigned char *)&byteOrder) != flipByteOrder);

        // Allocate memory and create a checkerboard pattern
        switch (imageDepth) {
            case 15:
                if (vis->red_mask == 0x001F) {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_rgb15_rev;
                    else
                        dataHandler = &CXDisplay::handleData_rgb15;
                }
                else {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_bgr15_rev;
                    else
                        dataHandler = &CXDisplay::handleData_bgr15;
                }
                imageData = malloc(width * height * sizeof(unsigned short));
                dests = (unsigned short *)imageData;

                for (y = 0; y < height; y++) {
                    for (x = 0; x < width; x++) {
                        int t = 0;

                        if ((x & 63) < 32)
                            t ^= 1;
                        if ((y & 63) < 32)
                            t ^= 1;

                        if (t) {
                            *dests++ = color_15_rgb(128, 128, 128, 128); // all chanels identical, so format irrelevant
                        }
                        else {
                            *dests++ = color_15_rgb(255, 255, 255, 255);
                        }
                    }
                }
                break;

            case 16:
                if (vis->red_mask == 0x001F) {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_rgb16_rev;
                    else
                        dataHandler = &CXDisplay::handleData_rgb16;
                }
                else {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_bgr16_rev;
                    else
                        dataHandler = &CXDisplay::handleData_bgr16;
                }
                imageData = malloc(width * height * sizeof(unsigned short));
                dests = (unsigned short *)imageData;

                for (y = 0; y < height; y++) {
                    for (x = 0; x < width; x++) {
                        int t = 0;

                        if ((x & 63) < 32)
                            t ^= 1;
                        if ((y & 63) < 32)
                            t ^= 1;

                        if (t) {
                            *dests++ = color_16_rgb(128, 128, 128, 128); // all chanels identical, so format irrelevant
                        }
                        else {
                            *dests++ = color_16_rgb(255, 255, 255, 255);
                        }
                    }
                }
                break;

            case 24:
            case 32:
                if (vis->red_mask == 0x000000FF) {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_abgr32;
                    else
                        dataHandler = &CXDisplay::handleData_rgba32;
                }
                else if (vis->red_mask == 0x0000FF00) {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_bgra32;
                    else
                        dataHandler = &CXDisplay::handleData_argb32;
                }
                else if (vis->red_mask == 0x00FF0000) {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_argb32;
                    else
                        dataHandler = &CXDisplay::handleData_bgra32;
                }
                else {
                    if (flipByteOrder)
                        dataHandler = &CXDisplay::handleData_rgba32;
                    else
                        dataHandler = &CXDisplay::handleData_abgr32;
                }
                imageData = malloc(width * height * sizeof(unsigned int));
                dest = (unsigned int *)imageData;

                for (y = 0; y < height; y++) {
                    for (x = 0; x < width; x++) {
                        int t = 0;

                        if ((x & 63) < 32)
                            t ^= 1;
                        if ((y & 63) < 32)
                            t ^= 1;

                        if (t) {
                            *dest++ = color_argb(128, 128, 128, 128); // all chanels identical, so format irrelevant
                        }
                        else {
                            *dest++ = color_argb(255, 255, 255, 255);
                        }
                    }
                }
                break;

            default:
                fprintf(stderr, "Unsupported sample format for framebuffer display\n");
                imageData = NULL;
                failure = true;
        }

        if (imageData != NULL) {
            displayName = strdup(name);
            exitedViaPipe = false;
            wakeup_pipe[0] = wakeup_pipe[1] = -1;
            if (pipe(wakeup_pipe) == -1) {
                fprintf(stderr, "openRender: Failed to create wakeup pipe for X11 driver\n");
                wakeup_pipe[0] = wakeup_pipe[1] = -1;
            }
            pthread_create(&thread, NULL, displayThread, this);
        }
    }
}

/*
 * Class: CXDisplay
 *
 * Method: ~CXDisplay (Destructor)
 *
 * Description:
 *     Properly clean up an object instance of this class
 *
 * Return:
 *
 * Notes:
 *
 */
CXDisplay::~CXDisplay() {
    free(displayName);
    if (wakeup_pipe[0] >= 0) close(wakeup_pipe[0]);
    if (wakeup_pipe[1] >= 0) close(wakeup_pipe[1]);
}

/*
 * Class: CXDisplay
 *
 * Method: main
 *
 * Description:
 *     The main display loop
 *
 * Return:
 *
 * Notes:
 *
 */
void CXDisplay::main() {
    assert(display != NULL);
    assert(failure == FALSE);

    windowUp = FALSE;
    windowDown = FALSE;

    XSetWindowAttributes window_attributes;
    unsigned long window_mask;
    int running = TRUE;

    window_attributes.border_pixel = BlackPixel(display, screen);
    window_attributes.background_pixel = BlackPixel(display, screen);
    window_attributes.override_redirect = 0;
    window_mask = CWBackPixel | CWBorderPixel;
    xcanvas = XCreateWindow(display,
                            DefaultRootWindow(display),
                            0,
                            0,
                            width, height,
                            0,
                            imageDepth,
                            InputOutput,
                            CopyFromParent,
                            window_mask,
                            &window_attributes);

    switch (imageDepth) {
        case 15:
        case 16:
            xim = XCreateImage(display,
                               CopyFromParent,
                               imageDepth,
                               ZPixmap,
                               0,
                               (char *)imageData,
                               width,
                               height,
                               16,
                               width * 2);
            break;
        case 24:
        case 32:
            xim = XCreateImage(display,
                               CopyFromParent,
                               imageDepth,
                               ZPixmap,
                               0,
                               (char *)imageData,
                               width,
                               height,
                               32,
                               width * 4);
            break;
    }

    image_gc = XCreateGC(display, xcanvas, 0, 0);

    XMapWindow(display, xcanvas);

    XSetWMProtocols(display, xcanvas, &WM_DELETE_WINDOW, 1);

    XWMHints *wm_hints = XAllocWMHints();
    if (wm_hints) {
        wm_hints->flags         = InputHint | StateHint;
        wm_hints->input         = True;
        wm_hints->initial_state = NormalState;
        XSetWMHints(display, xcanvas, wm_hints);
        XFree(wm_hints);
    }

    XClassHint *class_hint = XAllocClassHint();
    if (class_hint) {
        class_hint->res_name  = (char *)"openrender";
        class_hint->res_class = (char *)"OpenRender";
        XSetClassHint(display, xcanvas, class_hint);
        XFree(class_hint);
    }

    XChangeProperty(display, xcanvas, _NET_WM_NAME, UTF8_STRING, 8,
                    PropModeReplace,
                    (unsigned char *)displayName, (int)strlen(displayName));
    XStoreName(display, xcanvas, displayName);

    pid_t wm_pid = getpid();
    XChangeProperty(display, xcanvas, _NET_WM_PID, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&wm_pid, 1);

    XSelectInput(display, xcanvas, ExposureMask | StructureNotifyMask | KeyPressMask);

    // Initial Image

    XPutImage(display, xcanvas, image_gc, xim, 0, 0, 0, 0, width, height);
    XFlush(display);

    windowUp = TRUE;

    int x11_fd = ConnectionNumber(display);
    while (running) {
        // Drain all pending events (non-blocking)
        while (XPending(display)) {
            XEvent x_event;
            XNextEvent(display, &x_event);
            switch (x_event.type) {
                case Expose: {
                    XExposeEvent e = x_event.xexpose;
                    XPutImage(display, xcanvas, image_gc, xim,
                              e.x, e.y, e.x, e.y, e.width, e.height);
                    XFlush(display);
                } break;
                case KeyPress: {
                    KeySym key = XLookupKeysym(&x_event.xkey, 0);
                    if (key == XK_Escape || key == XK_q) running = FALSE;
                } break;
                case DestroyNotify:
                    running = FALSE;
                    break;
                case ClientMessage: {
                    const long *data = x_event.xclient.data.l;
                    if ((Atom)(data[0]) == WM_DELETE_WINDOW) running = FALSE;
                } break;
            }
        }
        if (!running) break;

        // Wait for an X11 event or a wakeup signal from finish()
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(x11_fd, &fds);
        int maxfd = x11_fd;
        if (wakeup_pipe[0] >= 0) {
            FD_SET(wakeup_pipe[0], &fds);
            if (wakeup_pipe[0] > maxfd) maxfd = wakeup_pipe[0];
        }
        int ret = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (ret == -1) {
            if (errno == EINTR) continue;
            break;
        }
        if (wakeup_pipe[0] >= 0 && FD_ISSET(wakeup_pipe[0], &fds)) {
            exitedViaPipe = true;
            break;
        }
    }

    windowDown = TRUE;
    if (!exitedViaPipe) {
        // Normal exit (user closed window): clean up X11 resources here
        XUnmapWindow(display, xcanvas);
        XFreeGC(display, image_gc);
        XDestroyImage(xim); // also frees imageData
        xim = NULL;
        XDestroyWindow(display, xcanvas);
        XCloseDisplay(display);
        display = NULL;
    }
    // If exitedViaPipe: leave all X11 resources open for the grandchild process
}

/*
 * Class: CXDisplay
 *
 * Method: finish
 *
 * Description:
 *     Finish displaying the data
 *
 * Return:
 *
 * Notes:
 *
 */
void CXDisplay::finish() {
    if (!windowDown) {
        XPutImage(display, xcanvas, image_gc, xim, 0, 0, 0, 0, width, height);
        const char *done_title = "openRender \xe2\x80\x94 Rendering Complete";
        XChangeProperty(display, xcanvas, _NET_WM_NAME, UTF8_STRING, 8,
                        PropModeReplace,
                        (unsigned char *)done_title, (int)strlen(done_title));
        XStoreName(display, xcanvas, done_title);
        XFlush(display);
    }

    // Interrupt the select-based event loop and wait for the thread to stop cleanly
    if (wakeup_pipe[1] >= 0)
        write(wakeup_pipe[1], "\0", 1);
    pthread_join(thread, NULL);
    // Thread has stopped. If user already closed the window, display==NULL already.

    if (!exitedViaPipe || !display) {
        // Window was closed by user before finish() was called — nothing to detach
        return;
    }

    // Double-fork: detach the window as an independent process so orender exits immediately
    pid_t pid = fork();
    if (pid < 0) return; // fork failed — graceful degradation
    if (pid > 0) {
        // Parent (orender): null X11 objects so the destructor becomes a no-op
        if (wakeup_pipe[0] >= 0) { close(wakeup_pipe[0]); wakeup_pipe[0] = -1; }
        if (wakeup_pipe[1] >= 0) { close(wakeup_pipe[1]); wakeup_pipe[1] = -1; }
        display = NULL;
        xim = NULL; // imageData is now owned by grandchild via xim
        waitpid(pid, NULL, 0);
        return;
    }

    // Intermediate child: fork grandchild then exit immediately (init will reap grandchild)
    pid_t pid2 = fork();
    if (pid2 > 0) _exit(0);

    // Grandchild: detached display process — keep window open until user closes it
    setsid();
    if (wakeup_pipe[0] >= 0) close(wakeup_pipe[0]);
    if (wakeup_pipe[1] >= 0) close(wakeup_pipe[1]);
    wakeup_pipe[0] = wakeup_pipe[1] = -1;

    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2);
        if (devnull > 2) close(devnull);
    }

    // Update _NET_WM_PID to reflect the new (grandchild) process
    pid_t gpid = getpid();
    XChangeProperty(display, xcanvas, _NET_WM_PID, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&gpid, 1);
    XFlush(display);

    // Simple blocking event loop — no wakeup pipe needed (single-threaded grandchild)
    int running2 = TRUE;
    while (running2) {
        XEvent x_event;
        XNextEvent(display, &x_event);
        switch (x_event.type) {
            case Expose: {
                XExposeEvent e = x_event.xexpose;
                XPutImage(display, xcanvas, image_gc, xim,
                          e.x, e.y, e.x, e.y, e.width, e.height);
                XFlush(display);
            } break;
            case KeyPress: {
                KeySym key = XLookupKeysym(&x_event.xkey, 0);
                if (key == XK_Escape || key == XK_q) running2 = FALSE;
            } break;
            case DestroyNotify:
                running2 = FALSE;
                break;
            case ClientMessage: {
                const long *data = x_event.xclient.data.l;
                if ((Atom)(data[0]) == WM_DELETE_WINDOW) running2 = FALSE;
            } break;
        }
    }

    XUnmapWindow(display, xcanvas);
    XFreeGC(display, image_gc);
    XDestroyImage(xim); // also frees imageData
    XDestroyWindow(display, xcanvas);
    XCloseDisplay(display);
    _exit(0);
}

/*
 * Class: CXDisplay
 *
 * Method: handleData_xxx
 *
 * Description:
 *     Stuff data into pixmap
 *
 * Return:
 *
 * Notes:
 *
 */

#define STUFF_MONO_ROW(colorPacker, v)                 \
    for (j = 0; j < w; j++) {                          \
        unsigned char dv = (unsigned char)((v) * 255); \
        *dest++ = colorPacker(dv, dv, dv, dv);         \
        src++;                                         \
    }

#define STUFF_ROW(colorPacker, r, g, b, inc)                 \
    for (j = 0; j < w; j++) {                                \
        unsigned char dr = (unsigned char)((r) * 255);       \
        unsigned char dg = (unsigned char)((g) * 255);       \
        unsigned char db = (unsigned char)((b) * 255);       \
        *dest++ = colorPacker(dr, dg, db, (unsigned char)0); \
        src += inc;                                          \
    }

#define STUFF_ALPHA_ROW(colorPacker, colorUnpacker, r, g, b, a, inc)           \
    for (j = 0; j < w; j++) {                                                  \
        colorUnpacker(dest[0]) float nr = (r) * (a) * 255 + (1 - (a)) * old_r; \
        float ng = (g) * (a) * 255 + (1 - (a)) * old_g;                        \
        float nb = (b) * (a) * 255 + (1 - (a)) * old_b;                        \
        float na = (a) * 255 + (1 - (a)) * old_a;                              \
        unsigned char dr = (unsigned char)nr;                                  \
        unsigned char dg = (unsigned char)ng;                                  \
        unsigned char db = (unsigned char)nb;                                  \
        unsigned char da = (unsigned char)na;                                  \
        *dest++ = colorPacker(dr, dg, db, da);                                 \
        src += inc;                                                            \
    }

#define STUFF_ALPHA_ROW_16(colorPacker, r, g, b, a, inc) \
    for (j = 0; j < w; j++) {                            \
        float nr = (r) * (a) * 255;                      \
        float ng = (g) * (a) * 255;                      \
        float nb = (b) * (a) * 255;                      \
        float na = (a) * 255;                            \
        unsigned char dr = (unsigned char)nr;            \
        unsigned char dg = (unsigned char)ng;            \
        unsigned char db = (unsigned char)nb;            \
        unsigned char da = (unsigned char)na; (void)da;   \
        *dest++ = colorPacker(dr, dg, db, da);           \
        src += inc;                                      \
    }

#define DEFINE_DATA_HANDLER_16(fn_name, colorPacker)                                              \
    void CXDisplay::handleData_##fn_name(int x, int y, int w, int h, float *d) {                  \
        int i, j;                                                                                 \
        switch (numSamples) {                                                                     \
            case 0:                                                                               \
                break;                                                                            \
            case 1:                                                                               \
                for (i = 0; i < h; i++) {                                                         \
                    const float *src = &d[i * w * numSamples];                                    \
                    unsigned short *dest = &((unsigned short *)imageData)[((i + y) * width + x)]; \
                    STUFF_MONO_ROW(colorPacker, src[0])                                           \
                }                                                                                 \
                break;                                                                            \
            case 2:                                                                               \
                for (i = 0; i < h; i++) {                                                         \
                    const float *src = &d[i * w * numSamples];                                    \
                    unsigned short *dest = &((unsigned short *)imageData)[((i + y) * width + x)]; \
                    STUFF_ALPHA_ROW_16(colorPacker, src[0], src[0], src[0], src[1], 2)            \
                }                                                                                 \
                break;                                                                            \
            case 3:                                                                               \
                for (i = 0; i < h; i++) {                                                         \
                    const float *src = &d[i * w * numSamples];                                    \
                    unsigned short *dest = &((unsigned short *)imageData)[((i + y) * width + x)]; \
                    STUFF_ROW(colorPacker, src[0], src[1], src[2], 3)                             \
                }                                                                                 \
                break;                                                                            \
            case 4:                                                                               \
                for (i = 0; i < h; i++) {                                                         \
                    const float *src = &d[i * w * numSamples];                                    \
                    unsigned short *dest = &((unsigned short *)imageData)[((i + y) * width + x)]; \
                    STUFF_ALPHA_ROW_16(colorPacker, src[0], src[1], src[2], src[3], 4)            \
                }                                                                                 \
                __attribute__((fallthrough));                                                     \
            default:                                                                              \
                for (i = 0; i < h; i++) {                                                         \
                    const float *src = &d[i * w * numSamples];                                    \
                    unsigned short *dest = &((unsigned short *)imageData)[((i + y) * width + x)]; \
                    STUFF_ALPHA_ROW_16(colorPacker, src[0], src[1], src[2], src[3], numSamples)   \
                }                                                                                 \
        }                                                                                         \
    }

#define DEFINE_DATA_HANDLER(fn_name, colorPacker, colorUnpacker)                                            \
    void CXDisplay::handleData_##fn_name(int x, int y, int w, int h, float *d) {                            \
        int i, j;                                                                                           \
        switch (numSamples) {                                                                               \
            case 0:                                                                                         \
                break;                                                                                      \
            case 1:                                                                                         \
                for (i = 0; i < h; i++) {                                                                   \
                    const float *src = &d[i * w * numSamples];                                              \
                    unsigned int *dest = &((unsigned int *)imageData)[((i + y) * width + x)];               \
                    STUFF_MONO_ROW(colorPacker, src[0])                                                     \
                }                                                                                           \
                break;                                                                                      \
            case 2:                                                                                         \
                for (i = 0; i < h; i++) {                                                                   \
                    const float *src = &d[i * w * numSamples];                                              \
                    unsigned int *dest = &((unsigned int *)imageData)[((i + y) * width + x)];               \
                    STUFF_ALPHA_ROW(colorPacker, colorUnpacker, src[0], src[0], src[0], src[1], 2)          \
                }                                                                                           \
                break;                                                                                      \
            case 3:                                                                                         \
                for (i = 0; i < h; i++) {                                                                   \
                    const float *src = &d[i * w * numSamples];                                              \
                    unsigned int *dest = &((unsigned int *)imageData)[((i + y) * width + x)];               \
                    STUFF_ROW(colorPacker, src[0], src[1], src[2], 3)                                       \
                }                                                                                           \
                break;                                                                                      \
            case 4:                                                                                         \
                for (i = 0; i < h; i++) {                                                                   \
                    const float *src = &d[i * w * numSamples];                                              \
                    unsigned int *dest = &((unsigned int *)imageData)[((i + y) * width + x)];               \
                    STUFF_ALPHA_ROW(colorPacker, colorUnpacker, src[0], src[1], src[2], src[3], 4)          \
                }                                                                                           \
                __attribute__((fallthrough));                                                               \
            default:                                                                                        \
                for (i = 0; i < h; i++) {                                                                   \
                    const float *src = &d[i * w * numSamples];                                              \
                    unsigned int *dest = &((unsigned int *)imageData)[((i + y) * width + x)];               \
                    STUFF_ALPHA_ROW(colorPacker, colorUnpacker, src[0], src[1], src[2], src[3], numSamples) \
                }                                                                                           \
        }                                                                                                   \
    }

DEFINE_DATA_HANDLER_16(rgb16, color_16_rgb)
DEFINE_DATA_HANDLER_16(rgb16_rev, color_16_rgb_rev)
DEFINE_DATA_HANDLER_16(bgr16, color_16_bgr)
DEFINE_DATA_HANDLER_16(bgr16_rev, color_16_bgr_rev)

DEFINE_DATA_HANDLER_16(rgb15, color_15_rgb)
DEFINE_DATA_HANDLER_16(rgb15_rev, color_15_rgb_rev)
DEFINE_DATA_HANDLER_16(bgr15, color_15_bgr)
DEFINE_DATA_HANDLER_16(bgr15_rev, color_15_bgr_rev)

DEFINE_DATA_HANDLER(rgba32, color_rgba, get_pix_rgba)
DEFINE_DATA_HANDLER(argb32, color_argb, get_pix_argb)
DEFINE_DATA_HANDLER(bgra32, color_bgra, get_pix_bgra)
DEFINE_DATA_HANDLER(abgr32, color_abgr, get_pix_abgr)

/*
 * Class: CXDisplay
 *
 * Method: data
 *
 * Description:
 *     Commit the image data
 *
 * Return:
 *
 * Notes:
 *
 */
int CXDisplay::data(int x, int y, int w, int h, float *d) {
    if (windowDown)
        return FALSE;

    clampData(w, h, d);

    (this->*dataHandler)(x, y, w, h, d); // call the appropriate pixel writer

    if (windowUp) {
        XPutImage(display, xcanvas, image_gc, xim, x, y, x, y, w, h);
        XFlush(display);
    }

    return TRUE;
}
