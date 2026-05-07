/**
 * fbq.h — macOS IPC display driver.
 *
 * CQDisplay spawns orender-fb-macos via posix_spawn and forwards tile data
 * over a Unix domain socket using the TLV protocol defined in fbipc.h.
 */

#ifndef FBQ_H
#define FBQ_H

#include "framebuffer.h"
#include "fbipc.h"
#include <mutex>
#include <sys/types.h>

class CQDisplay : public CDisplay {
public:
    CQDisplay(const char *name, const char *samples, int width, int height,
              int numSamples, const char *argv0);
    ~CQDisplay() override;

    int  data(int x, int y, int w, int h, float *d) override;
    void finish() override;

private:
    char    socketPath[256];
    int     socketFd;
    pid_t   helperPid;
    bool        disconnected;
    int         numSamplesVal;
    int         tilesSent = 0;
    std::mutex  writeMutex;
};

#endif // FBQ_H
