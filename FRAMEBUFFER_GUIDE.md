




macOS Quartz/Cocoa Driver (future phase)

     fork+Objective-C is unsafe on macOS ≥ 10.13 (ObjC runtime locks are not fork-safe). The
     double-fork pattern from Part A cannot be used directly. Instead: a helper-executable
     IPC model, which is also the most robust cross-platform approach long-term.

     Architecture

     orender (renderer process)
       framebuffer.so (display plugin)
         fbq.cpp (macOS driver)  ──── Unix socket ────  orender-fb (Cocoa helper exe)
         displayStart() → posix_spawn orender-fb,                NSWindow + NSImageView
                          connect to socket                      NSApplication main loop
         displayData()  → send pixel-data packets     →          redraw NSImageView
         displayFinish()→ send "done" packet,         →          retitle "Rendering Complete"
                          close socket, return                   keep window until user closes

     Protocol (TLV over Unix socket)

     ┌────────┬─────────────────────────┬──────────────────────────────────────┐
     │ Opcode │         Payload         │              Direction               │
     ├────────┼─────────────────────────┼──────────────────────────────────────┤
     │ START  │ width, height,          │ driver → helper                      │
     │        │ numSamples, title       │                                      │
     ├────────┼─────────────────────────┼──────────────────────────────────────┤
     │ DATA   │ x, y, w, h, float       │ driver → helper                      │
     │        │ pixels                  │                                      │
     ├────────┼─────────────────────────┼──────────────────────────────────────┤
     │ DONE   │ —                       │ driver → helper (window retitled;    │
     │        │                         │ helper keeps running)                │
     ├────────┼─────────────────────────┼──────────────────────────────────────┤
     │ QUIT   │ —                       │ either direction (graceful shutdown) │
     └────────┴─────────────────────────┴──────────────────────────────────────┘

     New files

     ┌───────────────────────────────────────────┬──────────────────────────────┐
     │                   File                    │           Purpose            │
     ├───────────────────────────────────────────┼──────────────────────────────┤
     │ src/framebuffer/fbq.h                     │ CQDisplay class declaration  │
     ├───────────────────────────────────────────┼──────────────────────────────┤
     │ src/framebuffer/fbq.cpp                   │ IPC client; posix_spawn the  │
     │                                           │ helper; socket send/recv     │
     ├───────────────────────────────────────────┼──────────────────────────────┤
     │ src/framebuffer/orender-fb/main.m         │ Cocoa app: NSWindow,         │
     │                                           │ NSImageView, socket server   │
     ├───────────────────────────────────────────┼──────────────────────────────┤
     │ src/framebuffer/orender-fb/CMakeLists.txt │ build orender-fb executable  │
     │                                           │ (macOS only)                 │
     ├───────────────────────────────────────────┼──────────────────────────────┤
     │ src/framebuffer/CMakeLists.txt            │ add APPLE branch for fbq     │
     │                                           │ module + orender-fb          │
     ├───────────────────────────────────────────┼──────────────────────────────┤
     │ src/framebuffer/framebuffer.cpp           │ add macOS platform selection │
     │                                           │  in displayStart()           │
     └───────────────────────────────────────────┴──────────────────────────────┘

     Linux migration path (optional, later)

     Once the helper-exe model is proven on macOS, the X11 and Wayland drivers can optionally
     migrate to it too, replacing the double-fork approach.
