/**
 * Project: openRender
 *
 * File: fbwl.h
 *
 * Description:
 *   This file defines the Wayland image displaying class.
 *   It is used to display the image on the screen using Wayland protocols.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef FBWL_H
#define FBWL_H

#include "framebuffer.h"
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#ifdef HAVE_LIBDECOR
#include <libdecor.h>
#endif
#include <pthread.h>
#include <queue>

/*
 * Struct: InputEvent
 */
struct InputEvent {
    enum Type { KEYBOARD, POINTER } type;
    uint32_t code;
    uint32_t state;
};

/*
 * Class: CWDisplay
 *
 * Description:
 *     The Wayland display class
 *
 * Notes:
 *
 */
class CWDisplay : public CDisplay {
    public:
        CWDisplay(const char *, const char *, int, int, int);
        ~CWDisplay();

        void main();
        int data(int, int, int, int, float *);
        void finish();

        // Wayland specific handles
        struct wl_display *display;
        struct wl_registry *registry;
        struct wl_compositor *compositor;
        struct wl_shm *shm;
        struct wp_fractional_scale_manager_v1 *fractional_scale_manager;
        struct wl_seat *seat;
        struct wl_surface *surface;
        struct wp_fractional_scale_v1 *fractional_scale;
#ifdef HAVE_LIBDECOR
        struct libdecor *libdecor_ctx;
        struct libdecor_frame *libdecor_frame;
        bool initialConfigured;
#else
        struct xdg_wm_base *xdg_wm_base;
        struct xdg_surface *xdg_surface;
        struct xdg_toplevel *xdg_toplevel;
#endif
        struct wl_keyboard *keyboard;
        struct wl_pointer *pointer;
        struct wl_buffer *buffer;
        
        void *shm_data;
        int pool_size;
        pthread_t thread;
        pthread_mutex_t mutex;
        int windowUp;
        int wakeup_pipe[2];
        std::queue<InputEvent> input_queue;
};

#endif
