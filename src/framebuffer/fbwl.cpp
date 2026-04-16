/**
 * Project: openRender
 *
 * File: fbwl.cpp
 *
 * Description:
 *   This file implements the Wayland image displaying class.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "fbwl.h"
#include "logging.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

static int set_cloexec_or_close(int fd) {
    long flags;
    if (fd == -1) return -1;
    flags = fcntl(fd, F_GETFD);
    if (flags == -1 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

static int create_tmpfile_cloexec(char *tmpname) {
    int fd;
    fd = mkstemp(tmpname);
    if (fd >= 0) {
        fd = set_cloexec_or_close(fd);
        unlink(tmpname);
    }
    return fd;
}

static int os_create_anonymous_file(off_t size) {
    static const char shm_template[] = "/openrender-shm-XXXXXX";
    const char *path;
    char *name;
    int fd;

    path = getenv("XDG_RUNTIME_DIR");
    if (!path) return -1;

    name = (char *)malloc(strlen(path) + sizeof(shm_template));
    strcpy(name, path);
    strcat(name, shm_template);

    fd = create_tmpfile_cloexec(name);
    free(name);

    if (fd < 0) return -1;
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/*
 * Static callbacks
 */
static void registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    (void)version;
    CWDisplay *d = (CWDisplay *)data;
    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor = (struct wl_compositor *)wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        d->shm = (struct wl_shm *)wl_registry_bind(registry, id, &wl_shm_interface, 1);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        d->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, "wp_fractional_scale_manager_v1") == 0) {
        d->fractional_scale_manager = (struct wp_fractional_scale_manager_v1 *)wl_registry_bind(registry, id, &wp_fractional_scale_manager_v1_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        d->seat = (struct wl_seat *)wl_registry_bind(registry, id, &wl_seat_interface, 1);
    }
}

static void registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    (void)data; (void)registry; (void)id;
}

static const struct wl_registry_listener registry_listener = {
    registry_handler,
    registry_remover
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    xdg_wm_base_ping
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    (void)xdg_toplevel; (void)states;
    CWDisplay *d = (CWDisplay *)data;
    if (width > 0 && height > 0 && (width != d->width || height != d->height)) {
        log_info("Wayland window resizing to {}x{}", width, height);
        pthread_mutex_lock(&d->mutex);
        
        if (d->buffer) wl_buffer_destroy(d->buffer);
        if (d->shm_data) munmap(d->shm_data, d->pool_size);

        d->width = width;
        d->height = height;

        int stride = d->width * 4;
        d->pool_size = stride * d->height;
        int fd = os_create_anonymous_file(d->pool_size);
        if (fd >= 0) {
            d->shm_data = mmap(NULL, d->pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (d->shm_data != MAP_FAILED) {
                struct wl_shm_pool *pool = wl_shm_create_pool(d->shm, fd, d->pool_size);
                d->buffer = wl_shm_pool_create_buffer(pool, 0, d->width, d->height, stride, WL_SHM_FORMAT_ARGB8888);
                wl_shm_pool_destroy(pool);
            } else {
                log_error("Failed to mmap SHM buffer for resize");
                d->shm_data = NULL;
                d->failure = TRUE;
            }
            close(fd);
        } else {
            log_error("Failed to create SHM file for resize");
            d->failure = TRUE;
        }

        pthread_mutex_unlock(&d->mutex);
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    (void)xdg_toplevel;
    CWDisplay *d = (CWDisplay *)data;
    log_info("Wayland window close requested");
    d->windowUp = FALSE;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    xdg_toplevel_configure,
    xdg_toplevel_close,
    NULL, // configure_bounds
    NULL  // wm_capabilities
};

static void fractional_scale_handle_preferred_scale(void *data, struct wp_fractional_scale_v1 *fractional_scale, uint32_t scale) {
    (void)fractional_scale;
    (void)data;
    log_debug("Wayland preferred fractional scale: {}", scale / 120.0f);
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    fractional_scale_handle_preferred_scale
};

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    (void)keyboard; (void)serial; (void)time;
    CWDisplay *d = (CWDisplay *)data;
    
    pthread_mutex_lock(&d->mutex);
    InputEvent ev = { InputEvent::KEYBOARD, key, state };
    d->input_queue.push(ev);
    
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && (key == 1 || key == 16)) {
        log_info("Exit key pressed, closing Wayland window");
        d->windowUp = FALSE;
    }
    pthread_mutex_unlock(&d->mutex);
}

static const struct wl_keyboard_listener keyboard_listener = {
    NULL, // keymap
    NULL, // enter
    NULL, // leave
    keyboard_handle_key,
    NULL, // modifiers
    NULL  // repeat_info
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    CWDisplay *d = (CWDisplay *)data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
        d->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
        wl_keyboard_destroy(d->keyboard);
        d->keyboard = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    NULL // name
};

static void *displayThread(void *data) {
    CWDisplay *d = (CWDisplay *)data;
    d->main();
    return NULL;
}

/*
 * Class: CWDisplay
 *
 * Method: CWDisplay (Constructor)
 *
 * Description:
 *
 * Return:
 *
 * Notes:
 *
 */
CWDisplay::CWDisplay(const char *name, const char *samples, int width, int height, int numSamples)
    : CDisplay(name, samples, width, height, numSamples)
{
    this->display = NULL;
    this->registry = NULL;
    this->compositor = NULL;
    this->shm = NULL;
    this->xdg_wm_base = NULL;
    this->fractional_scale_manager = NULL;
    this->seat = NULL;
    this->surface = NULL;
    this->xdg_surface = NULL;
    this->xdg_toplevel = NULL;
    this->fractional_scale = NULL;
    this->keyboard = NULL;
    this->pointer = NULL;
    this->buffer = NULL;
    this->shm_data = NULL;
    this->pool_size = 0;
    this->windowUp = FALSE;

    pthread_mutex_init(&this->mutex, NULL);

    log_info("Initializing Wayland display driver for window '{}'", name);

    this->display = wl_display_connect(NULL);
    if (!this->display) {
        log_error("Failed to connect to Wayland display");
        this->failure = TRUE;
        return;
    }

    this->registry = wl_display_get_registry(this->display);
    wl_registry_add_listener(this->registry, &registry_listener, this);
    wl_display_roundtrip(this->display);

    if (!this->compositor || !this->shm || !this->xdg_wm_base) {
        log_error("Wayland compositor, SHM, or XDG WM Base not available");
        this->failure = TRUE;
        return;
    }

    xdg_wm_base_add_listener(this->xdg_wm_base, &xdg_wm_base_listener, this);

    if (this->seat) {
        wl_seat_add_listener(this->seat, &seat_listener, this);
    }

    this->surface = wl_compositor_create_surface(this->compositor);
    
    if (this->fractional_scale_manager) {
        this->fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(this->fractional_scale_manager, this->surface);
        wp_fractional_scale_v1_add_listener(this->fractional_scale, &fractional_scale_listener, this);
    }

    this->xdg_surface = xdg_wm_base_get_xdg_surface(this->xdg_wm_base, this->surface);
    xdg_surface_add_listener(this->xdg_surface, &xdg_surface_listener, this);

    this->xdg_toplevel = xdg_surface_get_toplevel(this->xdg_surface);
    xdg_toplevel_set_title(this->xdg_toplevel, name);
    xdg_toplevel_add_listener(this->xdg_toplevel, &xdg_toplevel_listener, this);

    wl_surface_commit(this->surface);
    wl_display_roundtrip(this->display);

    // Buffer allocation (T009)
    int stride = width * 4;
    this->pool_size = stride * height;
    int fd = os_create_anonymous_file(this->pool_size);
    if (fd >= 0) {
        this->shm_data = mmap(NULL, this->pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (this->shm_data != MAP_FAILED) {
            struct wl_shm_pool *pool = wl_shm_create_pool(this->shm, fd, this->pool_size);
            this->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
            wl_shm_pool_destroy(pool);
        } else {
            log_error("Failed to mmap SHM buffer");
            this->shm_data = NULL;
            this->failure = TRUE;
        }
        close(fd);
    } else {
        log_error("Failed to create anonymous SHM file");
        this->failure = TRUE;
    }

    this->windowUp = TRUE;
    pthread_create(&this->thread, NULL, displayThread, this);
    log_info("Wayland display driver initialized successfully ({}x{})", width, height);
}

/*
 * Class: CWDisplay
 *
 * Method: ~CWDisplay (Destructor)
 *
 * Description:
 *
 * Return:
 *
 * Notes:
 *
 */
CWDisplay::~CWDisplay() {
    log_info("Shutting down Wayland display driver");
    this->windowUp = FALSE;
    pthread_join(this->thread, NULL);

    if (this->buffer) wl_buffer_destroy(this->buffer);
    if (this->shm_data) munmap(this->shm_data, this->pool_size);
    if (this->fractional_scale) wp_fractional_scale_v1_destroy(this->fractional_scale);
    if (this->keyboard) wl_keyboard_destroy(this->keyboard);
    if (this->pointer) wl_pointer_destroy(this->pointer);
    if (this->xdg_toplevel) xdg_toplevel_destroy(this->xdg_toplevel);
    if (this->xdg_surface) xdg_surface_destroy(this->xdg_surface);
    if (this->surface) wl_surface_destroy(this->surface);
    if (this->seat) wl_seat_destroy(this->seat);
    if (this->fractional_scale_manager) wp_fractional_scale_manager_v1_destroy(this->fractional_scale_manager);
    if (this->xdg_wm_base) xdg_wm_base_destroy(this->xdg_wm_base);
    if (this->compositor) wl_compositor_destroy(this->compositor);
    if (this->shm) wl_shm_destroy(this->shm);
    if (this->registry) wl_registry_destroy(this->registry);
    if (this->display) wl_display_disconnect(this->display);

    pthread_mutex_destroy(&this->mutex);
}

/*
 * Class: CWDisplay
 *
 * Method: main
 *
 * Description:
 *     The event loop thread entry point.
 *
 * Return:
 *
 * Notes:
 *
 */
void CWDisplay::main() {
    while (this->windowUp) {
        if (wl_display_dispatch(this->display) == -1) {
            log_error("Wayland display dispatch error, terminating event loop");
            this->windowUp = FALSE;
            break;
        }
    }
}

/*
 * Class: CWDisplay
 *
 * Method: data
 *
 * Description:
 *     Receives image data.
 *
 * Return:
 *     On Success: TRUE
 *     On Failure: FALSE
 *
 * Notes:
 *
 */
int CWDisplay::data(int x, int y, int w, int h, float *data) {
    if (this->failure || !this->windowUp) return FALSE;

    pthread_mutex_lock(&this->mutex);

    if (!this->shm_data) {
        pthread_mutex_unlock(&this->mutex);
        return FALSE;
    }

    uint32_t *pixels = (uint32_t *)this->shm_data;
    
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int target_x = x + i;
            int target_y = y + j;
            
            if (target_x < 0 || target_x >= width || target_y < 0 || target_y >= height) continue;

            float *src = &data[(j * w + i) * numSamples];
            
            uint32_t r = (uint32_t)(src[0] < 0 ? 0 : (src[0] > 1 ? 255 : src[0] * 255));
            uint32_t g = (uint32_t)(src[1] < 0 ? 0 : (src[1] > 1 ? 255 : src[1] * 255));
            uint32_t b = (uint32_t)(src[2] < 0 ? 0 : (src[2] > 1 ? 255 : src[2] * 255));
            uint32_t a = 255;
            if (numSamples > 3) {
                a = (uint32_t)(src[3] < 0 ? 0 : (src[3] > 1 ? 255 : src[3] * 255));
            }

            pixels[target_y * width + target_x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    wl_surface_attach(this->surface, this->buffer, 0, 0);
    wl_surface_damage(this->surface, x, y, w, h);
    wl_surface_commit(this->surface);
    wl_display_flush(this->display);

    pthread_mutex_unlock(&this->mutex);

    return TRUE;
}

/*
 * Class: CWDisplay
 *
 * Method: finish
 *
 * Description:
 *     Finishes receiving image data.
 *
 * Return:
 *
 * Notes:
 *
 */
void CWDisplay::finish() {
    this->windowUp = FALSE;
}
