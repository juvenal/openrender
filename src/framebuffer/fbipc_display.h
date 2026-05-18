/**
 * fbipc_display.h — unified IPC display driver.
 *
 * CIPCDisplay spawns orender-fb via posix_spawn and forwards tile data
 * over a Unix domain socket using the TLV protocol defined in fbipc.h.
 * Works on macOS (helper: orender-fb-macos) and Linux (helper: orender-fb-linux).
 * The helper handles all OS-specific windowing: Cocoa/Metal on macOS,
 * X11 or Wayland (auto-detected via WAYLAND_DISPLAY) on Linux.
 */

#ifndef FBIPC_DISPLAY_H
#define FBIPC_DISPLAY_H

#include "framebuffer.h"
#include "fbipc.h"
#include <mutex>
#include <sys/types.h>

class CIPCDisplay : public CDisplay {
public:
    CIPCDisplay(const char *name, const char *samples,
                int width, int height, int numSamples);
    ~CIPCDisplay() override;

    int  data(int x, int y, int w, int h, float *d) override;
    void finish() override;

private:
    char       socketPath[256];
    int        socketFd;
    pid_t      helperPid;
    bool       disconnected;
    int        numSamplesVal;
    int        tilesSent;
    std::mutex writeMutex;
};

#endif // FBIPC_DISPLAY_H
