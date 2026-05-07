/**
 * fbwl.h — Linux Wayland IPC display driver.
 *
 * CWDisplay spawns orender-fb-linux via posix_spawn and forwards tile data
 * over a Unix domain socket using the TLV protocol defined in fbipc.h.
 * The helper process handles all Wayland (and X11 fallback) display logic.
 */

#ifndef FBWL_H
#define FBWL_H

#include "framebuffer.h"
#include "fbipc.h"
#include <mutex>
#include <sys/types.h>

class CWDisplay : public CDisplay {
public:
    CWDisplay(const char *name, const char *samples, int width, int height, int numSamples);
    ~CWDisplay() override;

    int  data(int x, int y, int w, int h, float *d) override;
    void finish() override;

private:
    char    socketPath[256];
    int     socketFd;
    pid_t   helperPid;
    bool        disconnected;
    int         numSamplesVal;
    std::mutex  writeMutex;
};

#endif // FBWL_H
