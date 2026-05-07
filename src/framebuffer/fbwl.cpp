/**
 * fbwl.cpp — Linux Wayland IPC display driver.
 *
 * Spawns orender-fb-linux via posix_spawn and streams TLV packets to it.
 * The helper auto-detects Wayland via WAYLAND_DISPLAY and falls back to X11.
 * If the helper fails to launch or times out, renders without display
 * (data() always returns TRUE).
 */

#include "fbwl.h"
#include "fbipc.h"
#include "common/global.h"

#include <cstring>
#include <cstdio>
#include <cerrno>
#include <csignal>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <spawn.h>

extern char **environ;

#define TRUE  1
#define FALSE 0

// ---------------------------------------------------------------------------
// Helper: get this process's executable path (Linux via /proc/self/exe)
// ---------------------------------------------------------------------------

static std::string getWlExePath() {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return std::string();
}

// ---------------------------------------------------------------------------
// Helper: connect to socket with retry until timeoutSecs elapses
// ---------------------------------------------------------------------------

static int wlConnectWithTimeout(const char *sockPath, int timeoutSecs) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);

    const int sleepUsec  = 50000;
    const int maxRetries = timeoutSecs * (1000000 / sleepUsec);

    for (int i = 0; i < maxRetries; i++) {
        if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0)
            return fd;
        if (errno != ENOENT && errno != ECONNREFUSED) break;
        usleep(sleepUsec);
    }
    close(fd);
    return -1;
}

// ---------------------------------------------------------------------------
// CWDisplay constructor
// ---------------------------------------------------------------------------

CWDisplay::CWDisplay(const char *name, const char *samples, int width, int height,
                     int numSamples)
    : CDisplay(name, samples, width, height, numSamples),
      socketFd(-1), helperPid(-1), disconnected(false),
      numSamplesVal(numSamples)
{
    std::string sockStr = makeSocketPath(getpid());
    strncpy(socketPath, sockStr.c_str(), sizeof(socketPath) - 1);
    socketPath[sizeof(socketPath) - 1] = '\0';

    std::string exePath    = getWlExePath();
    std::string helperPath = makeHelperPath(exePath.c_str(), "orender-fb-linux");

    char helperPathBuf[4096];
    char socketPathBuf[256];
    strncpy(helperPathBuf, helperPath.c_str(), sizeof(helperPathBuf) - 1);
    strncpy(socketPathBuf, socketPath,         sizeof(socketPathBuf) - 1);
    char *helperArgv[3] = { helperPathBuf, socketPathBuf, nullptr };

    // Spawn in a new process group so Ctrl-C (SIGINT to orender's pgid)
    // does not also kill the helper window.
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0); // child PGID = child PID

    pid_t pid;
    int spawnErr = posix_spawnp(&pid, helperArgv[0], nullptr, &attr,
                                helperArgv, environ);
    posix_spawnattr_destroy(&attr);

    if (spawnErr != 0) {
        fprintf(stderr,
                "openRender: framebuffer display unavailable — "
                "could not spawn orender-fb-linux (%s): %s\n",
                helperPath.c_str(), strerror(spawnErr));
        disconnected = true;
        return;
    }
    helperPid = pid;

    socketFd = wlConnectWithTimeout(socketPath, 5);
    if (socketFd < 0) {
        fprintf(stderr,
                "openRender: framebuffer display unavailable — "
                "socket connect timed out (%s)\n", socketPath);
        kill(helperPid, SIGTERM);
        helperPid    = -1;
        disconnected = true;
        return;
    }

    if (!sendStart(socketFd, static_cast<uint32_t>(width),
                   static_cast<uint32_t>(height),
                   static_cast<uint32_t>(numSamples), name)) {
        fprintf(stderr, "openRender: framebuffer display — START send failed\n");
        close(socketFd);
        socketFd     = -1;
        disconnected = true;
    }
}

CWDisplay::~CWDisplay() {
    if (socketFd >= 0) {
        close(socketFd);
        socketFd = -1;
    }
}

// ---------------------------------------------------------------------------
// data() — send DATA packet; always returns TRUE (display loss is non-fatal)
// ---------------------------------------------------------------------------

int CWDisplay::data(int x, int y, int w, int h, float *d) {
    if (disconnected || socketFd < 0) return TRUE;

    clampData(w, h, d); // operates on caller-owned buffer — safe before lock

    signal(SIGPIPE, SIG_IGN);

    // Serialize socket writes: concurrent render threads would interleave TLV
    // header and payload bytes, corrupting the protocol.
    std::lock_guard<std::mutex> lock(writeMutex);
    if (disconnected || socketFd < 0) return TRUE; // re-check under lock

    if (!sendData(socketFd, static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                  static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                  static_cast<uint32_t>(numSamplesVal), d)) {
        disconnected = true;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// finish() — send DONE and close; helper keeps window open independently
// ---------------------------------------------------------------------------

void CWDisplay::finish() {
    if (!disconnected && socketFd >= 0) {
        signal(SIGPIPE, SIG_IGN);
        sendDone(socketFd);
        close(socketFd);
        socketFd = -1;
    }
    // Helper continues running independently — do NOT wait for it
}
