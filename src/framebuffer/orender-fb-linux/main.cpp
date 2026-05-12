/**
 * orender-fb-linux/main.cpp
 *
 * Standalone helper binary that displays a framebuffer window on Linux
 * (X11 or Wayland). Spawned by the orender framebuffer driver (fbx.cpp or
 * fbwl.cpp), receives pixel tiles over a Unix-domain socket using the
 * TLV IPC protocol defined in fbipc.h.
 *
 * Architecture: the helper is the socket SERVER.  The orender driver is the
 * client that connects to this process.  The helper binds to the fixed per-user
 * socket path (argv[1]) and keeps the server socket open between renders so
 * successive orender runs reuse the same helper process.
 *
 * Each render session is served on its own pthread.  The thread creates a
 * display window (X11 or Wayland), receives DATA tiles, and keeps the window
 * open after DONE until the user closes it.  Multiple windows may be visible
 * simultaneously.  The helper exits when:
 *   - All windows have been closed by the user (windowCount drops to zero), or
 *   - A QUIT packet is received (closes all windows).
 *
 * Wire protocol: same as the macOS helper — see specs/004-macos-framebuffer-output/
 *
 * Usage: orender-fb-linux <socket-path>
 */

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "framebuffer/fbipc.h"

// ---------------------------------------------------------------------------
// Global state shared between the accept loop and window threads
// ---------------------------------------------------------------------------

static std::atomic<int>  g_windowCount{0}; // number of currently open windows
static std::atomic<bool> g_quit{false};    // set to true to stop accepting

// ---------------------------------------------------------------------------
// isWaylandAvailable: probe WAYLAND_DISPLAY env var (X11 fallback otherwise)
// ---------------------------------------------------------------------------

static bool isWaylandAvailable() {
    const char *wd = getenv("WAYLAND_DISPLAY");
    return wd != nullptr && wd[0] != '\0';
}

// ---------------------------------------------------------------------------
// Shared session state (populated from START, updated via DATA/DONE)
// ---------------------------------------------------------------------------

struct SessionCtx {
    uint32_t width      = 0;
    uint32_t height     = 0;
    uint32_t numSamples = 0;
    char     title[512] = {};
    char     baseTitle[512] = {};  // original title before status prefix
    int      tileCount  = 0;
    bool     done       = false;
    bool     interrupted = false;
    bool     quit       = false;
};

// ---------------------------------------------------------------------------
// Pixel clamp helper: clamp floats to [0,1]
// ---------------------------------------------------------------------------

static inline uint8_t toU8(float f) {
    if (f <= 0.0f) return 0;
    if (f >= 1.0f) return 255;
    return static_cast<uint8_t>(f * 255.0f + 0.5f);
}

// ---------------------------------------------------------------------------
// Low-level socket read: reads exactly n bytes, returns false on EOF/error
// ---------------------------------------------------------------------------

static bool readExact(int fd, void *buf, size_t n) {
    uint8_t *p = static_cast<uint8_t *>(buf);
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= static_cast<size_t>(r);
    }
    return true;
}

// ---------------------------------------------------------------------------
// X11 Window implementation
// ---------------------------------------------------------------------------

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#define color_argb(r, g, b, a) ((b << 24) | (g << 16) | (r << 8) | a)

// Returns true  → session ended normally (DONE/interrupted); main re-accepts.
// Returns false → QUIT packet received; caller sets g_quit.
static bool runX11Window(int sockfd, SessionCtx &ctx) {
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        fprintf(stderr, "orender-fb-linux: cannot open X11 display\n");
        return true; // not a QUIT, just a display failure
    }

    Atom WM_DELETE_WINDOW    = XInternAtom(display, "WM_DELETE_WINDOW",    False);
    Atom WM_PROTOCOLS        = XInternAtom(display, "WM_PROTOCOLS",        False);
    Atom _NET_WM_NAME        = XInternAtom(display, "_NET_WM_NAME",        False);
    Atom _NET_WM_PID         = XInternAtom(display, "_NET_WM_PID",         False);
    Atom _NET_WM_USER_TIME   = XInternAtom(display, "_NET_WM_USER_TIME",   False);
    Atom UTF8_STRING         = XInternAtom(display, "UTF8_STRING",         False);
    (void)WM_PROTOCOLS;

    int screen = DefaultScreen(display);
    int depth  = DefaultDepth(display, screen);

    // Allocate backing image buffer (always 32bpp ARGB for simplicity)
    size_t pixBytes = static_cast<size_t>(ctx.width) * ctx.height * 4;
    void *imageData = malloc(pixBytes);
    if (!imageData) {
        XCloseDisplay(display);
        return true;
    }

    // Checkerboard init
    auto *dest32 = static_cast<uint32_t *>(imageData);
    for (uint32_t y = 0; y < ctx.height; ++y) {
        for (uint32_t x = 0; x < ctx.width; ++x) {
            int t = (((x >> 5) ^ (y >> 5)) & 1);
            uint8_t v = t ? 128 : 200;
            dest32[y * ctx.width + x] = color_argb(v, v, v, 255);
        }
    }

    XSetWindowAttributes wa;
    wa.border_pixel    = BlackPixel(display, screen);
    wa.background_pixel = BlackPixel(display, screen);
    wa.override_redirect = 0;

    Window xcanvas = XCreateWindow(display, DefaultRootWindow(display),
                                   0, 0, ctx.width, ctx.height, 0,
                                   depth, InputOutput, CopyFromParent,
                                   CWBackPixel | CWBorderPixel, &wa);

    XImage *xim = XCreateImage(display, CopyFromParent, static_cast<unsigned>(depth),
                                ZPixmap, 0, static_cast<char *>(imageData),
                                ctx.width, ctx.height, 32, ctx.width * 4);

    GC gc = XCreateGC(display, xcanvas, 0, nullptr);

    // Set _NET_WM_USER_TIME to 0 before mapping to tell the WM this window was
    // not opened by a recent user interaction — prevents focus stealing.
    unsigned long zero = 0;
    XChangeProperty(display, xcanvas, _NET_WM_USER_TIME, XA_CARDINAL, 32,
                    PropModeReplace, reinterpret_cast<const unsigned char *>(&zero), 1);

    XMapWindow(display, xcanvas);
    XSetWMProtocols(display, xcanvas, &WM_DELETE_WINDOW, 1);

    XWMHints *wm_hints = XAllocWMHints();
    if (wm_hints) {
        wm_hints->flags = InputHint | StateHint;
        wm_hints->input = True;
        wm_hints->initial_state = NormalState;
        XSetWMHints(display, xcanvas, wm_hints);
        XFree(wm_hints);
    }

    XClassHint *ch = XAllocClassHint();
    if (ch) {
        ch->res_name  = const_cast<char *>("openrender");
        ch->res_class = const_cast<char *>("OpenRender");
        XSetClassHint(display, xcanvas, ch);
        XFree(ch);
    }

    XChangeProperty(display, xcanvas, _NET_WM_NAME, UTF8_STRING, 8,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char *>(ctx.title),
                    static_cast<int>(strlen(ctx.title)));
    XStoreName(display, xcanvas, ctx.title);
    pid_t mypid = getpid();
    XChangeProperty(display, xcanvas, _NET_WM_PID, XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<unsigned char *>(&mypid), 1);

    XSelectInput(display, xcanvas, ExposureMask | StructureNotifyMask | KeyPressMask);
    XPutImage(display, xcanvas, gc, xim, 0, 0, 0, 0, ctx.width, ctx.height);
    XFlush(display);

    int x11_fd = ConnectionNumber(display);
    bool running = true;
    bool socketOpen = true;

    std::vector<uint8_t> pbuf;

    while (running) {
        // Check for global quit signal from another thread
        if (g_quit.load(std::memory_order_relaxed)) {
            running = false;
            break;
        }

        // Drain pending X11 events without blocking
        while (XPending(display) > 0) {
            XEvent ev;
            XNextEvent(display, &ev);
            switch (ev.type) {
            case Expose: {
                XExposeEvent &e = ev.xexpose;
                XPutImage(display, xcanvas, gc, xim, e.x, e.y, e.x, e.y, e.width, e.height);
                XFlush(display);
                break;
            }
            case KeyPress: {
                KeySym key = XLookupKeysym(&ev.xkey, 0);
                if (key == XK_Escape || key == XK_q) running = false;
                break;
            }
            case DestroyNotify:
                running = false;
                break;
            case ClientMessage: {
                const long *d = ev.xclient.data.l;
                if (static_cast<Atom>(d[0]) == WM_DELETE_WINDOW) {
                    // Attempt to notify driver (harmless if driver already exited)
                    if (socketOpen) sendQuit(sockfd);
                    running = false;
                }
                break;
            }
            }
        }
        if (!running) break;

        if (!socketOpen && (ctx.done || ctx.interrupted)) {
            // Window stays until user closes it — keep event loop running
            struct pollfd pf = { x11_fd, POLLIN, 0 };
            poll(&pf, 1, 50);
            continue;
        }

        // Poll both the X11 fd and the socket
        struct pollfd pfs[2];
        pfs[0] = { x11_fd, POLLIN, 0 };
        pfs[1] = { socketOpen ? sockfd : -1, POLLIN, 0 };
        int n = poll(pfs, 2, 50);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (socketOpen && (pfs[1].revents & (POLLIN | POLLHUP | POLLERR))) {
            FBHeader hdr;
            if (!readExact(sockfd, &hdr, sizeof(hdr))) {
                // EOF / disconnect
                socketOpen = false;
                if (!ctx.done && !ctx.quit) {
                    if (ctx.tileCount > 0) {
                        ctx.interrupted = true;
                        snprintf(ctx.title, sizeof(ctx.title),
                                 "Interrupted \xe2\x80\x94 %s", ctx.baseTitle);
                    } else {
                        // No tiles received — treat as spurious connect; close
                        running = false;
                    }
                    XChangeProperty(display, xcanvas, _NET_WM_NAME, UTF8_STRING, 8,
                                    PropModeReplace,
                                    reinterpret_cast<const unsigned char *>(ctx.title),
                                    static_cast<int>(strlen(ctx.title)));
                    XStoreName(display, xcanvas, ctx.title);
                    XFlush(display);
                }
                continue;
            }

            uint32_t len = hdr.length;
            pbuf.resize(len);
            if (len > 0 && !readExact(sockfd, pbuf.data(), len)) {
                socketOpen = false;
                ctx.interrupted = true;
                continue;
            }

            switch (hdr.opcode) {
            case FBOpcode::DATA: {
                if (len < sizeof(FBDataPayload)) break;
                auto *dp = reinterpret_cast<const FBDataPayload *>(pbuf.data());
                uint32_t tx = dp->x, ty = dp->y, tw = dp->w, th = dp->h;
                const float *src = reinterpret_cast<const float *>(
                    pbuf.data() + sizeof(FBDataPayload));
                uint32_t ns = ctx.numSamples;

                for (uint32_t row = 0; row < th; ++row) {
                    uint32_t iy = ty + row;
                    if (iy >= ctx.height) break;
                    auto *dst = dest32 + iy * ctx.width + tx;
                    for (uint32_t col = 0; col < tw; ++col) {
                        if (tx + col >= ctx.width) break;
                        const float *p = src + (row * tw + col) * ns;
                        uint8_t r = toU8(p[0]);
                        uint8_t g = (ns >= 3) ? toU8(p[1]) : r;
                        uint8_t b = (ns >= 3) ? toU8(p[2]) : r;
                        *dst++ = color_argb(r, g, b, 255);
                    }
                }
                XPutImage(display, xcanvas, gc, xim,
                          static_cast<int>(tx), static_cast<int>(ty),
                          static_cast<int>(tx), static_cast<int>(ty),
                          tw, th);
                XFlush(display);
                ctx.tileCount++;
                break;
            }
            case FBOpcode::DONE:
                ctx.done = true;
                socketOpen = false;
                snprintf(ctx.title, sizeof(ctx.title),
                         "Rendering Complete \xe2\x80\x94 %s", ctx.baseTitle);
                XChangeProperty(display, xcanvas, _NET_WM_NAME, UTF8_STRING, 8,
                                PropModeReplace,
                                reinterpret_cast<const unsigned char *>(ctx.title),
                                static_cast<int>(strlen(ctx.title)));
                XStoreName(display, xcanvas, ctx.title);
                XPutImage(display, xcanvas, gc, xim, 0, 0, 0, 0, ctx.width, ctx.height);
                XFlush(display);
                break;
            case FBOpcode::QUIT:
                ctx.quit = true;
                socketOpen = false;
                running = false; // close window immediately on QUIT
                break;
            default:
                socketOpen = false;
                break;
            }
        }
    }

    XUnmapWindow(display, xcanvas);
    XFreeGC(display, gc);
    XDestroyImage(xim); // also frees imageData
    xim       = nullptr;
    imageData = nullptr;
    XDestroyWindow(display, xcanvas);
    XCloseDisplay(display);

    return !ctx.quit; // false = QUIT → caller sets g_quit; true = normal/interrupted
}

// ---------------------------------------------------------------------------
// Wayland Window implementation
// ---------------------------------------------------------------------------

#ifdef HAVE_WAYLAND
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#ifdef HAVE_LIBDECOR
#include <libdecor-0/libdecor.h>
#endif

struct WlCtx {
    struct wl_display    *display   = nullptr;
    struct wl_registry   *registry  = nullptr;
    struct wl_compositor *compositor = nullptr;
    struct wl_shm        *shm       = nullptr;
    struct wl_seat       *seat      = nullptr;
#ifndef HAVE_LIBDECOR
    struct xdg_wm_base   *xdg_wm_base  = nullptr;
    struct xdg_surface   *xdg_surface  = nullptr;
    struct xdg_toplevel  *xdg_toplevel = nullptr;
#endif
#ifdef HAVE_LIBDECOR
    struct libdecor       *decor       = nullptr;
    struct libdecor_frame *decor_frame = nullptr;
#endif
    struct wl_surface  *surface   = nullptr;
    struct wl_buffer   *buffer    = nullptr;
    struct wl_keyboard *keyboard  = nullptr;
    void               *shm_data = nullptr;
    int                 shm_fd   = -1;
    int                 shm_size = 0;
    uint32_t            width    = 0;
    uint32_t            height   = 0;
    uint32_t            numSamples = 0;
    bool                running  = true;
    bool                configured = false;
    bool                needQuit = false; // window closed by user
};

// Wayland SHM helpers
static int set_cloexec(int fd) {
    long flags = fcntl(fd, F_GETFD);
    if (flags == -1) { close(fd); return -1; }
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) { close(fd); return -1; }
    return fd;
}
static int os_create_anonymous_file(off_t size) {
    const char *path = getenv("XDG_RUNTIME_DIR");
    if (!path) return -1;
    std::string name = std::string(path) + "/openrender-shm-XXXXXX";
    int fd = set_cloexec(mkstemp(const_cast<char *>(name.c_str())));
    unlink(name.c_str());
    if (fd < 0) return -1;
    if (ftruncate(fd, size) < 0) { close(fd); return -1; }
    return fd;
}

static bool wlCreateShmBuffer(WlCtx &w) {
    w.shm_size = static_cast<int>(w.width * w.height * 4);
    w.shm_fd   = os_create_anonymous_file(w.shm_size);
    if (w.shm_fd < 0) return false;
    w.shm_data = mmap(nullptr, w.shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, w.shm_fd, 0);
    if (w.shm_data == MAP_FAILED) { close(w.shm_fd); w.shm_fd = -1; return false; }

    // Checkerboard init
    auto *p = static_cast<uint32_t *>(w.shm_data);
    for (uint32_t y = 0; y < w.height; ++y) {
        for (uint32_t x = 0; x < w.width; ++x) {
            int t = (((x >> 5) ^ (y >> 5)) & 1);
            uint8_t v = t ? 128 : 200;
            p[y * w.width + x] = (0xFF000000u) | (uint32_t(v) << 16) | (uint32_t(v) << 8) | v;
        }
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(w.shm, w.shm_fd, w.shm_size);
    w.buffer = wl_shm_pool_create_buffer(pool, 0, w.width, w.height,
                                          w.width * 4, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    return true;
}

// Registry callbacks — all use the data pointer (no shared global)
static void registry_handler(void *data, struct wl_registry *reg, uint32_t id,
                              const char *iface, uint32_t version) {
    (void)version;
    auto &w = *static_cast<WlCtx *>(data);
    if (strcmp(iface, "wl_compositor") == 0)
        w.compositor = static_cast<struct wl_compositor *>(
            wl_registry_bind(reg, id, &wl_compositor_interface, 1));
    else if (strcmp(iface, "wl_shm") == 0)
        w.shm = static_cast<struct wl_shm *>(
            wl_registry_bind(reg, id, &wl_shm_interface, 1));
    else if (strcmp(iface, "wl_seat") == 0)
        w.seat = static_cast<struct wl_seat *>(
            wl_registry_bind(reg, id, &wl_seat_interface, 1));
#ifndef HAVE_LIBDECOR
    else if (strcmp(iface, "xdg_wm_base") == 0)
        w.xdg_wm_base = static_cast<struct xdg_wm_base *>(
            wl_registry_bind(reg, id, &xdg_wm_base_interface, 1));
#endif
}
static void registry_remover(void *, struct wl_registry *, uint32_t) {}
static const struct wl_registry_listener registry_listener = { registry_handler, registry_remover };

// Keyboard callbacks
static void kb_keymap(void *, struct wl_keyboard *, uint32_t, int32_t, uint32_t) {}
static void kb_enter(void *, struct wl_keyboard *, uint32_t, struct wl_surface *, struct wl_array *) {}
static void kb_leave(void *, struct wl_keyboard *, uint32_t, struct wl_surface *) {}
static void kb_key(void *data, struct wl_keyboard *, uint32_t, uint32_t, uint32_t key, uint32_t state) {
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && (key == 1 /* ESC */ || key == 16 /* Q */)) {
        static_cast<WlCtx *>(data)->running  = false;
        static_cast<WlCtx *>(data)->needQuit = true;
    }
}
static void kb_modifiers(void *, struct wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
static void kb_repeat_info(void *, struct wl_keyboard *, int32_t, int32_t) {}
static const struct wl_keyboard_listener kb_listener = {
    kb_keymap, kb_enter, kb_leave, kb_key, kb_modifiers, kb_repeat_info
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    auto &w = *static_cast<WlCtx *>(data);
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !w.keyboard) {
        w.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(w.keyboard, &kb_listener, &w);
    }
}
static void seat_name(void *, struct wl_seat *, const char *) {}
static const struct wl_seat_listener seat_listener = { seat_capabilities, seat_name };

#ifndef HAVE_LIBDECOR
static void xdg_wm_base_ping(void *, struct xdg_wm_base *base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}
static const struct xdg_wm_base_listener xdg_wm_base_listener = { xdg_wm_base_ping };

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surf, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surf, serial);
    static_cast<WlCtx *>(data)->configured = true;
}
static const struct xdg_surface_listener xdg_surface_listener = { xdg_surface_configure };

static void toplevel_configure(void *, struct xdg_toplevel *, int32_t, int32_t, struct wl_array *) {}
static void toplevel_close(void *data, struct xdg_toplevel *) {
    static_cast<WlCtx *>(data)->running  = false;
    static_cast<WlCtx *>(data)->needQuit = true;
}
static const struct xdg_toplevel_listener toplevel_listener = { toplevel_configure, toplevel_close };
#endif // !HAVE_LIBDECOR

#ifdef HAVE_LIBDECOR
static void decor_configure(struct libdecor_frame *frame,
                            struct libdecor_configuration *config, void *data) {
    auto &w = *static_cast<WlCtx *>(data);
    struct libdecor_state *state = libdecor_state_new(w.width, w.height);
    libdecor_frame_commit(frame, state, config);
    libdecor_state_free(state);
    w.configured = true;
}
static void decor_close(struct libdecor_frame *, void *data) {
    static_cast<WlCtx *>(data)->running  = false;
    static_cast<WlCtx *>(data)->needQuit = true;
}
static void decor_commit(struct libdecor_frame *, void *) {}
static struct libdecor_frame_interface decor_frame_iface = {
    decor_configure, decor_close, decor_commit,
    nullptr, nullptr, nullptr, nullptr, nullptr
};
#endif

static void wlCommitFrame(WlCtx &w) {
    wl_surface_attach(w.surface, w.buffer, 0, 0);
    wl_surface_damage(w.surface, 0, 0, w.width, w.height);
    wl_surface_commit(w.surface);
    wl_display_flush(w.display);
}

// Returns true  → session ended normally; main re-accepts.
// Returns false → QUIT packet received; caller sets g_quit.
static bool runWaylandWindow(int sockfd, SessionCtx &ctx) {
    WlCtx w;
    w.width      = ctx.width;
    w.height     = ctx.height;
    w.numSamples = ctx.numSamples;

    w.display = wl_display_connect(nullptr);
    if (!w.display) {
        fprintf(stderr, "orender-fb-linux: cannot connect to Wayland display\n");
        return true; // not a QUIT, just a display failure
    }

    w.registry = wl_display_get_registry(w.display);
    wl_registry_add_listener(w.registry, &registry_listener, &w);
    wl_display_roundtrip(w.display);
    wl_display_roundtrip(w.display);

    if (!w.compositor || !w.shm) {
        fprintf(stderr, "orender-fb-linux: required Wayland globals not available\n");
        wl_display_disconnect(w.display);
        return true;
    }

    if (!wlCreateShmBuffer(w)) {
        wl_display_disconnect(w.display);
        return true;
    }

    w.surface = wl_compositor_create_surface(w.compositor);

#ifdef HAVE_LIBDECOR
    w.decor = libdecor_new(w.display, nullptr);
    w.decor_frame = libdecor_decorate(w.decor, w.surface, &decor_frame_iface, &w);
    libdecor_frame_set_app_id(w.decor_frame, "openrender");
    libdecor_frame_set_title(w.decor_frame, ctx.title);
    libdecor_frame_map(w.decor_frame);
#else
    if (!w.xdg_wm_base) {
        fprintf(stderr, "orender-fb-linux: xdg_wm_base not available\n");
        goto cleanup;
    }
    xdg_wm_base_add_listener(w.xdg_wm_base, &xdg_wm_base_listener, nullptr);
    w.xdg_surface  = xdg_wm_base_get_xdg_surface(w.xdg_wm_base, w.surface);
    w.xdg_toplevel = xdg_surface_get_toplevel(w.xdg_surface);
    xdg_surface_add_listener(w.xdg_surface, &xdg_surface_listener, &w);
    xdg_toplevel_add_listener(w.xdg_toplevel, &toplevel_listener, &w);
    xdg_toplevel_set_app_id(w.xdg_toplevel, "openrender");
    xdg_toplevel_set_title(w.xdg_toplevel, ctx.title);
    wl_surface_commit(w.surface);
#endif

    if (w.seat)
        wl_seat_add_listener(w.seat, &seat_listener, &w);

    // Wait for configure
    while (!w.configured && wl_display_dispatch(w.display) > 0) {}
    wlCommitFrame(w);

    {
        int wl_fd = wl_display_get_fd(w.display);
        std::vector<uint8_t> pbuf;
        bool socketOpen = true;

        while (w.running) {
            // Check for global quit signal from another thread
            if (g_quit.load(std::memory_order_relaxed)) {
                w.running = false;
                break;
            }

            wl_display_flush(w.display);

            struct pollfd pfs[2];
            pfs[0] = { wl_fd,                            POLLIN, 0 };
            pfs[1] = { socketOpen ? sockfd : -1, POLLIN, 0 };
            int n = poll(pfs, 2, 50);
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }

            if (pfs[0].revents & POLLIN) {
                if (wl_display_dispatch(w.display) < 0) break;
            }

            if (socketOpen && (pfs[1].revents & (POLLIN | POLLHUP | POLLERR))) {
                FBHeader hdr;
                if (!readExact(sockfd, &hdr, sizeof(hdr))) {
                    socketOpen = false;
                    if (!ctx.done && !ctx.quit) {
                        if (ctx.tileCount > 0) {
                            ctx.interrupted = true;
                            snprintf(ctx.title, sizeof(ctx.title),
                                     "Interrupted \xe2\x80\x94 %s", ctx.baseTitle);
                        } else {
                            w.running = false; // no tiles → don't show window
                        }
#ifdef HAVE_LIBDECOR
                        libdecor_frame_set_title(w.decor_frame, ctx.title);
#else
                        xdg_toplevel_set_title(w.xdg_toplevel, ctx.title);
#endif
                        wlCommitFrame(w);
                    }
                    continue;
                }

                uint32_t len = hdr.length;
                pbuf.resize(len);
                if (len > 0 && !readExact(sockfd, pbuf.data(), len)) {
                    socketOpen = false;
                    ctx.interrupted = true;
                    continue;
                }

                switch (hdr.opcode) {
                case FBOpcode::DATA: {
                    if (len < sizeof(FBDataPayload)) break;
                    auto *dp = reinterpret_cast<const FBDataPayload *>(pbuf.data());
                    const float *src = reinterpret_cast<const float *>(
                        pbuf.data() + sizeof(FBDataPayload));
                    auto *pix = static_cast<uint32_t *>(w.shm_data);
                    uint32_t ns = ctx.numSamples;

                    for (uint32_t row = 0; row < dp->h; ++row) {
                        uint32_t iy = dp->y + row;
                        if (iy >= w.height) break;
                        for (uint32_t col = 0; col < dp->w; ++col) {
                            uint32_t ix = dp->x + col;
                            if (ix >= w.width) break;
                            const float *p = src + (row * dp->w + col) * ns;
                            uint8_t r = toU8(p[0]);
                            uint8_t g = (ns >= 3) ? toU8(p[1]) : r;
                            uint8_t b = (ns >= 3) ? toU8(p[2]) : r;
                            pix[iy * w.width + ix] = (0xFF000000u)
                                | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
                        }
                    }
                    wl_surface_attach(w.surface, w.buffer, 0, 0);
                    wl_surface_damage(w.surface, dp->x, dp->y, dp->w, dp->h);
                    wl_surface_commit(w.surface);
                    wl_display_flush(w.display);
                    ctx.tileCount++;
                    break;
                }
                case FBOpcode::DONE:
                    ctx.done = true;
                    socketOpen = false;
                    snprintf(ctx.title, sizeof(ctx.title),
                             "Rendering Complete \xe2\x80\x94 %s", ctx.baseTitle);
#ifdef HAVE_LIBDECOR
                    libdecor_frame_set_title(w.decor_frame, ctx.title);
#else
                    xdg_toplevel_set_title(w.xdg_toplevel, ctx.title);
#endif
                    wlCommitFrame(w);
                    break;
                case FBOpcode::QUIT:
                    ctx.quit     = true;
                    socketOpen   = false;
                    w.running    = false; // close window immediately on QUIT
                    break;
                default:
                    socketOpen = false;
                    break;
                }
            }
        }
    }

    // Attempt to notify driver if user closed window mid-render (harmless if driver exited)
    if (w.needQuit) sendQuit(sockfd);

#ifndef HAVE_LIBDECOR
    cleanup:
    if (w.xdg_toplevel) xdg_toplevel_destroy(w.xdg_toplevel);
    if (w.xdg_surface)  xdg_surface_destroy(w.xdg_surface);
#endif
#ifdef HAVE_LIBDECOR
    if (w.decor_frame) libdecor_frame_unref(w.decor_frame);
    if (w.decor)       libdecor_unref(w.decor);
#endif
    if (w.keyboard)  wl_keyboard_destroy(w.keyboard);
    if (w.buffer)    wl_buffer_destroy(w.buffer);
    if (w.surface)   wl_surface_destroy(w.surface);
    if (w.shm_data && w.shm_data != MAP_FAILED) munmap(w.shm_data, w.shm_size);
    if (w.shm_fd >= 0) close(w.shm_fd);
    if (w.shm)       wl_shm_destroy(w.shm);
    if (w.compositor) wl_compositor_destroy(w.compositor);
    if (w.registry)  wl_registry_destroy(w.registry);
    wl_display_disconnect(w.display);

    return !ctx.quit;
}

#endif // HAVE_WAYLAND

// ---------------------------------------------------------------------------
// Session thread — one per accepted connection (one window per render)
// ---------------------------------------------------------------------------

struct ThreadArg {
    int clientFd;
};

static void *sessionThread(void *raw) {
    auto *ta = static_cast<ThreadArg *>(raw);
    int clientFd = ta->clientFd;
    delete ta;

    // Read START packet (first packet must be START or QUIT)
    FBHeader hdr;
    if (!readExact(clientFd, &hdr, sizeof(hdr))) {
        close(clientFd);
        g_windowCount.fetch_sub(1, std::memory_order_relaxed);
        return nullptr;
    }

    if (hdr.opcode == FBOpcode::QUIT) {
        // Driver requesting helper exit before any render started
        g_quit.store(true, std::memory_order_relaxed);
        close(clientFd);
        g_windowCount.fetch_sub(1, std::memory_order_relaxed);
        return nullptr;
    }

    if (hdr.opcode != FBOpcode::START) {
        fprintf(stderr, "orender-fb-linux: expected START packet, got opcode %d\n",
                static_cast<int>(hdr.opcode));
        close(clientFd);
        g_windowCount.fetch_sub(1, std::memory_order_relaxed);
        return nullptr;
    }

    // Read START payload
    uint32_t payloadLen = hdr.length;
    std::vector<uint8_t> startBuf(payloadLen);
    if (payloadLen > 0 && !readExact(clientFd, startBuf.data(), payloadLen)) {
        fprintf(stderr, "orender-fb-linux: truncated START payload\n");
        close(clientFd);
        g_windowCount.fetch_sub(1, std::memory_order_relaxed);
        return nullptr;
    }

    if (payloadLen < sizeof(FBStartPayload)) {
        fprintf(stderr, "orender-fb-linux: START payload too small\n");
        close(clientFd);
        g_windowCount.fetch_sub(1, std::memory_order_relaxed);
        return nullptr;
    }

    auto *sp = reinterpret_cast<const FBStartPayload *>(startBuf.data());

    SessionCtx ctx;
    ctx.width      = sp->width;
    ctx.height     = sp->height;
    ctx.numSamples = sp->numSamples;

    uint32_t titleLen = sp->titleLen;
    if (titleLen > 0 && payloadLen >= sizeof(FBStartPayload) + titleLen) {
        size_t copy = (titleLen < sizeof(ctx.title) - 1) ? titleLen : sizeof(ctx.title) - 1;
        memcpy(ctx.title, startBuf.data() + sizeof(FBStartPayload), copy);
        ctx.title[copy] = '\0';
    } else {
        strncpy(ctx.title, "openRender", sizeof(ctx.title) - 1);
    }
    strncpy(ctx.baseTitle, ctx.title, sizeof(ctx.baseTitle) - 1);

    // Run the appropriate display window; it blocks until the user closes it
    bool continueServing;
#ifdef HAVE_WAYLAND
    if (isWaylandAvailable()) {
        continueServing = runWaylandWindow(clientFd, ctx);
    } else {
        continueServing = runX11Window(clientFd, ctx);
    }
#else
    continueServing = runX11Window(clientFd, ctx);
#endif

    if (!continueServing) {
        // QUIT received — stop the outer accept loop
        g_quit.store(true, std::memory_order_relaxed);
    }

    close(clientFd);
    g_windowCount.fetch_sub(1, std::memory_order_relaxed);
    return nullptr;
}

// ---------------------------------------------------------------------------
// Main — server: bind socket, accept connections, spawn per-session threads
// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    if (argc < 2) {
        fputs("usage: orender-fb-linux <socket-path>\n", stderr);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    const char *socketPath = argv[1];

    // Create and bind the server socket
    int serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("orender-fb-linux: socket");
        return 1;
    }

    // Remove any stale socket file from a previous (crashed) helper
    unlink(socketPath);

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

    if (bind(serverFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        perror("orender-fb-linux: bind");
        close(serverFd);
        return 1;
    }
    if (listen(serverFd, 5) != 0) {
        perror("orender-fb-linux: listen");
        close(serverFd);
        unlink(socketPath);
        return 1;
    }

    bool windowsEverOpened = false;

    // Outer accept loop — one iteration per render session.
    // Exits when: QUIT received (g_quit), or all windows closed after at least
    // one render (windowsEverOpened && windowCount == 0).
    while (!g_quit.load(std::memory_order_relaxed)) {
        // Exit once the user has closed all open windows
        if (windowsEverOpened &&
            g_windowCount.load(std::memory_order_relaxed) == 0) {
            break;
        }

        // Poll with timeout so we can check g_quit / windowCount periodically
        struct pollfd pf = { serverFd, POLLIN, 0 };
        int r = poll(&pf, 1, 500); // 500 ms
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (r == 0) continue; // timeout — loop back to check flags
        if (!(pf.revents & POLLIN)) break;

        int clientFd = accept(serverFd, nullptr, nullptr);
        if (clientFd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // Increment windowCount before spawning so the check above is safe
        g_windowCount.fetch_add(1, std::memory_order_relaxed);
        windowsEverOpened = true;

        auto *ta = new ThreadArg { clientFd };
        pthread_t tid;
        if (pthread_create(&tid, nullptr, sessionThread, ta) != 0) {
            perror("orender-fb-linux: pthread_create");
            close(clientFd);
            delete ta;
            g_windowCount.fetch_sub(1, std::memory_order_relaxed);
        } else {
            pthread_detach(tid);
        }
    }

    close(serverFd);
    unlink(socketPath);

    // Wait for any remaining window threads to finish (they check g_quit and
    // will close their windows within one poll timeout ≈ 50 ms)
    while (g_windowCount.load(std::memory_order_relaxed) > 0) {
        usleep(100000); // 100 ms
    }

    return 0;
}
