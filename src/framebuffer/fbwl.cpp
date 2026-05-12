/**
 * fbwl.cpp — Linux Wayland IPC display driver.
 *
 * Spawns orender-fb via posix_spawn and streams TLV packets to it.
 * The helper auto-detects Wayland via WAYLAND_DISPLAY and falls back to X11.
 * Tries to reuse an existing helper before spawning; uses a fixed per-user
 * socket path so successive renders share one helper process.
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
// Helper: single non-blocking connect attempt to an existing helper.
// Returns a connected fd on success, -1 if no helper is listening.
// The helper keeps its server socket open between renders (outer accept loop),
// so this succeeds when the helper is idle after finishing a previous render.
// ---------------------------------------------------------------------------

static int wlTryConnectExisting(const char *sockPath) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    // Use memcpy + explicit bound to avoid -Wformat-truncation / -Wstringop-truncation:
    // on Linux sun_path is 108 bytes; socketPath[] is 256 bytes — the paths we generate
    // are always short, but GCC's inliner sees the declaration sizes and warns.
    {
        size_t n = strlen(sockPath);
        if (n >= sizeof(addr.sun_path)) n = sizeof(addr.sun_path) - 1;
        memcpy(addr.sun_path, sockPath, n);
        addr.sun_path[n] = '\0';
    }

    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0)
        return fd;
    close(fd);
    return -1;
}

// ---------------------------------------------------------------------------
// Helper: connect to socket with retry until timeoutSecs elapses
// ---------------------------------------------------------------------------

static int wlConnectWithTimeout(const char *sockPath, int timeoutSecs) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    // Use memcpy + explicit bound to avoid -Wformat-truncation / -Wstringop-truncation:
    // on Linux sun_path is 108 bytes; socketPath[] is 256 bytes — the paths we generate
    // are always short, but GCC's inliner sees the declaration sizes and warns.
    {
        size_t n = strlen(sockPath);
        if (n >= sizeof(addr.sun_path)) n = sizeof(addr.sun_path) - 1;
        memcpy(addr.sun_path, sockPath, n);
        addr.sun_path[n] = '\0';
    }

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
    // Fixed socket path per user — shared across renders so successive orender
    // invocations reuse the same helper process and window.
    std::string sockStr = makeFixedSocketPath();
    snprintf(socketPath, sizeof(socketPath), "%s", sockStr.c_str());

    // Try to connect to an existing helper that is idle between renders.
    // The helper keeps its server socket open (outer accept loop), so this
    // succeeds whenever a previous render has finished but the window is
    // still visible.  On success we skip the spawn entirely.
    socketFd = wlTryConnectExisting(socketPath);

    if (socketFd < 0) {
        // No existing helper is listening — remove any stale socket file and
        // spawn a fresh helper.
        unlink(socketPath);

        std::string exePath    = getWlExePath();
        std::string helperPath = makeHelperPath(exePath.c_str(), "orender-fb");

        char helperPathBuf[4096];
        char socketPathBuf[256];
        snprintf(helperPathBuf, sizeof(helperPathBuf), "%s", helperPath.c_str());
        snprintf(socketPathBuf, sizeof(socketPathBuf), "%s", socketPath);
        char *helperArgv[3] = { helperPathBuf, socketPathBuf, nullptr };

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

        // Prevent helper zombies and implicit wait during orender exit.
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

        socketFd = wlConnectWithTimeout(socketPath, 5);
        if (socketFd < 0) {
            fprintf(stderr,
                    "openRender: framebuffer display unavailable — "
                    "socket connect timed out (%s)\n", socketPath);
            kill(helperPid, SIGTERM);
            helperPid = -1;
            failure = TRUE;
            return;
        }
    }

    // Send START packet (to either reused or freshly spawned helper)
    signal(SIGPIPE, SIG_IGN);
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
