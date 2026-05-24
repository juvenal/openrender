# Linux Verification: orender-wire and orender-fb

## Date: 2026-05-24
## Platform: Linux (Generic x86_64, Wayland/X11)
## Status: **PASS**

This document records the successful verification of the interactive wireframe previewer (`orender-wire`) and the framebuffer display driver (`orender-fb`) on Linux after the GTK4 operational refactor and dependency downgrade.

---

## 1. orender-wire (GTK 4 / OpenGL 3.3 Core)

### Verification Targets
- [X] GTK 4.20 Compatibility (Dependency Downgrade)
- [X] Mesa/EGL Warning Suppression
- [X] Terminal Detachment (Background Execution)
- [X] RPATH / Shared Library Resolution (No `LD_LIBRARY_PATH`)
- [X] Multi-instance Support

### Test Execution & Results

| Test Case | Command | Expected Result | Result |
|-----------|---------|-----------------|--------|
| **Dependency Check** | `ldd orender-wire` | Links against `libgtk-4.so.1` (verified version 4.20+ on target). | **PASS** |
| **Clean Startup** | `./orender-wire scene.rib` | No `libEGL warning` or `ZINK` errors printed to console. | **PASS** |
| **Terminal Detachment** | `./orender-wire scene.rib` | Shell prompt returns immediately; window stays open. | **PASS** |
| **RPATH Check** | `readelf -d orender-wire` | Contains `RUNPATH: $ORIGIN/../lib`. Loads `libri.so` from relative path. | **PASS** |
| **Multi-instance** | Launch `fileA.rib` then `fileB.rib`. | Two independent windows open with correct files. | **PASS** |
| **GL Rendering** | Manual inspection. | Arcball navigation, grid, and wireframe primitives render correctly. | **PASS** |

### Fix Notes
- **Mesa Warnings**: Silenced via `setenv` for `EGL_LOG_LEVEL=fatal`, `MESA_DEBUG=silent`, and `LIBGL_DEBUG=quiet` in `main()`.
- **D-Bus Single-Instance**: Disabled via `G_APPLICATION_NON_UNIQUE` flag to prevent the second process from delegating to the first.

---

## 2. orender-fb (Unified Framebuffer IPC)

### Verification Targets
- [X] Shared Memory IPC (Renderer-to-GUI)
- [X] Display Driver Lifecycle (RiBegin/RiEnd)
- [X] Linux X11/Wayland Backend Parity

### Test Execution & Results

| Test Case | Command | Expected Result | Result |
|-----------|---------|-----------------|--------|
| **Standard Render** | `orender scene.rib` (display "framebuffer") | Window opens, image displays progressively via IPC. | **PASS** |
| **Driver Exit** | `RiEnd()` | Window stays open until closed by user; renderer process exits. | **PASS** |
| **Environment** | `LD_LIBRARY_PATH` unset. | `orender` finds `displays/framebuffer.so` and `libri.so` via RPATH. | **PASS** |

---

## 3. Installation Layout Verification

Verified that `make install` produces the following structure on Linux:

```text
/usr/local/
├── bin/
│   ├── orender
│   └── orender-wire -> ../libexec/orender-wire  # Symlink
├── lib/
│   ├── libri.so
│   └── librslo.so
├── libexec/
│   └── orender-wire                             # Binary
├── displays/
│   └── framebuffer.so
└── ...
```

**Conclusion**: The Linux GUI stack is now stable, silent, and compliant with the project's installation conventions.
