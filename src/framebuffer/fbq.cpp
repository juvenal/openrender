/**
 * fbq.cpp — macOS IPC display driver.
 *
 * Spawns orender-fb via posix_spawn and streams TLV packets to it.
 * If the helper fails to launch or the socket times out, emits a warning
 * and renders without display (data() always returns TRUE).
 */

#include "fbq.h"
#include "fbipc.h"
#include "common/global.h"
#include "logging.hpp"

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
#include <libproc.h>

extern char **environ;

#define TRUE  1
#define FALSE 0

// ---------------------------------------------------------------------------
// Helper: get this process's executable path (macOS)
// Use proc_pidpath() — stays in libSystem, avoids CoreServices.
// _NSGetExecutablePath pulls CoreServices, which initialises background
// framework threads that block main() from returning normally.
// ---------------------------------------------------------------------------

static std::string getExePath() {
    char buf[PROC_PIDPATHINFO_MAXSIZE];
    int ret = proc_pidpath(getpid(), buf, sizeof(buf));
    if (ret > 0) {
        return std::string(buf);
    }
    return std::string();
}

// ---------------------------------------------------------------------------
// Helper: connect with timeout (retry loop, up to timeoutSecs)
// ---------------------------------------------------------------------------

static int connectWithTimeout(const char *socketPath, int timeoutSecs) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

    const int sleepUsec  = 50000; // 50 ms
    const int maxRetries = timeoutSecs * (1000000 / sleepUsec);

    for (int i = 0; i < maxRetries; i++) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            return fd; // connected
        }
        if (errno != ENOENT && errno != ECONNREFUSED) break;
        usleep(sleepUsec);
    }
    close(fd);
    return -1;
}

// ---------------------------------------------------------------------------
// Helper: single non-blocking connect attempt to an existing helper.
// Returns a connected fd on success, -1 if no helper is listening.
// The helper keeps its server socket open between renders (outer accept loop),
// so this succeeds when the helper is idle after finishing a previous render.
// ---------------------------------------------------------------------------

static int tryConnectExisting(const char *socketPath) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        return fd;
    }
    close(fd);
    return -1;
}

// ---------------------------------------------------------------------------
// CQDisplay constructor
// ---------------------------------------------------------------------------

CQDisplay::CQDisplay(const char *name, const char *samples, int width, int height,
                     int numSamples, const char *argv0)
    : CDisplay(name, samples, width, height, numSamples),
      socketFd(-1), helperPid(-1), disconnected(false),
      numSamplesVal(numSamples)
{
    // Fixed socket path per user — shared across renders so successive orender
    // invocations reuse the same helper process and window.
    std::string sockStr = makeFixedSocketPath();
    strncpy(socketPath, sockStr.c_str(), sizeof(socketPath) - 1);
    socketPath[sizeof(socketPath) - 1] = '\0';

    // Try to connect to an existing helper that is idle between renders.
    // The helper keeps its server socket open (outer accept loop), so this
    // succeeds whenever a previous render has finished but the window is
    // still visible.  On success we skip the spawn entirely.
    socketFd = tryConnectExisting(socketPath);

    if (socketFd < 0) {
        // No existing helper is listening — remove any stale socket file and
        // spawn a fresh helper.
        log_debug("no existing helper found; spawning orender-fb");
        unlink(socketPath);

        std::string exePath  = (argv0 && argv0[0]) ? std::string(argv0) : getExePath();
        std::string helperPath = makeHelperPath(exePath.c_str(), "orender-fb");

        char *helperArgv[3];
        char helperPathBuf[4096];
        char socketPathBuf[256];
        strncpy(helperPathBuf, helperPath.c_str(), sizeof(helperPathBuf) - 1);
        strncpy(socketPathBuf, socketPath, sizeof(socketPathBuf) - 1);
        helperArgv[0] = helperPathBuf;
        helperArgv[1] = socketPathBuf;
        helperArgv[2] = nullptr;

        // Spawn in a new process group so Ctrl-C (SIGINT to orender's pgid)
        // does not also kill the helper window.
        posix_spawnattr_t attr;
        posix_spawnattr_init(&attr);
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attr, 0); // child PGID = child PID

        // Redirect the helper's stdin/stdout/stderr to /dev/null.  The helper
        // is a GUI app; it never reads stdin or writes diagnostics to the
        // terminal.  Closing these fds prevents the child from holding
        // orender's controlling terminal open, which would block orender's
        // C-runtime stdio cleanup after main() returns in an interactive shell.
        posix_spawn_file_actions_t file_actions;
        posix_spawn_file_actions_init(&file_actions);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            posix_spawn_file_actions_adddup2(&file_actions, devnull, STDIN_FILENO);
            posix_spawn_file_actions_adddup2(&file_actions, devnull, STDOUT_FILENO);
            posix_spawn_file_actions_adddup2(&file_actions, devnull, STDERR_FILENO);
            posix_spawn_file_actions_addclose(&file_actions, devnull);
        }

        // Ignore SIGCHLD so the helper does not accumulate as a zombie when it
        // exits, and so the C runtime does not implicitly block waiting for it
        // during orender's own exit sequence.
        signal(SIGCHLD, SIG_IGN);

        pid_t pid;
        int spawnErr = posix_spawnp(&pid, helperArgv[0], &file_actions, &attr,
                                    helperArgv, environ);
        posix_spawnattr_destroy(&attr);
        posix_spawn_file_actions_destroy(&file_actions);
        if (devnull >= 0) close(devnull);

        if (spawnErr != 0) {
            fprintf(stderr,
                    "openRender: framebuffer display unavailable — "
                    "could not spawn orender-fb (%s): %s\n",
                    helperPath.c_str(), strerror(spawnErr));
            failure = TRUE;
            return;
        }
        helperPid = pid;
        log_debug("spawned orender-fb pid={}", (int)pid);

        socketFd = connectWithTimeout(socketPath, 5);
        if (socketFd < 0) {
            fprintf(stderr,
                    "openRender: framebuffer display unavailable — "
                    "socket connect timed out (%s)\n", socketPath);
            kill(helperPid, SIGTERM);
            helperPid = -1;
            failure = TRUE;
            return;
        }
    } else {
        log_debug("reusing existing orender-fb helper");
    }

    // Send START packet (to either reused or freshly spawned helper)
    signal(SIGPIPE, SIG_IGN);
    if (!sendStart(socketFd, (uint32_t)width, (uint32_t)height,
                   (uint32_t)numSamples, name)) {
        fprintf(stderr, "openRender: framebuffer display — START send failed\n");
        close(socketFd);
        socketFd    = -1;
        disconnected = true;
    }
}

CQDisplay::~CQDisplay() {
    if (socketFd >= 0) {
        close(socketFd);
        socketFd = -1;
    }
}

// ---------------------------------------------------------------------------
// data() — send DATA packet; always returns TRUE (display failure not fatal)
// ---------------------------------------------------------------------------

int CQDisplay::data(int x, int y, int w, int h, float *d) {
    if (disconnected || socketFd < 0) return TRUE;

    clampData(w, h, d); // operates on caller-owned buffer — safe before lock

    signal(SIGPIPE, SIG_IGN);

    // Serialize socket writes: concurrent render threads would interleave TLV
    // header and payload bytes, corrupting the protocol.
    std::lock_guard<std::mutex> lock(writeMutex);
    if (disconnected || socketFd < 0) return TRUE; // re-check under lock

    tilesSent++;
    if (tilesSent == 1)
        log_debug("first tile sent ({},{} {}x{})", x, y, w, h);

    if (!sendData(socketFd, (uint32_t)x, (uint32_t)y,
                  (uint32_t)w, (uint32_t)h, (uint32_t)numSamplesVal, d)) {
        log_debug("sendData failed after {} tiles", tilesSent);
        disconnected = true;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// finish() — send DONE, close socket; renderer can exit immediately after
// ---------------------------------------------------------------------------

void CQDisplay::finish() {
    log_debug("finish: disconnected={} socketFd={} tilesSent={}", disconnected, socketFd, tilesSent);
    if (!disconnected && socketFd >= 0) {
        signal(SIGPIPE, SIG_IGN);
        log_debug("finish: sending DONE");
        sendDone(socketFd);
        log_debug("finish: DONE sent, closing socket");
        close(socketFd);
        socketFd = -1;
        log_debug("finish: socket closed");
    }
    log_debug("finish: returning");
    // Helper continues running independently — we do NOT wait for it
}
