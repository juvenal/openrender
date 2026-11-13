/**
 * tests/gi_split/gi_loopback.h
 *
 * Spec 032: the socketpair() loopback of the remote-channel tests, for the tests that come after T-A (T-A2). The same two
 * helpers T-A (test_gi_remote_loopback.cpp, approved and recorded) carries privately; T-A is not edited.
 *
 *   captureSend(send, result)   runs `send` on a thread into one end of a socketpair and returns every byte that arrives
 *                               at the other end
 *   replayInto(bytes, receive)  writes `bytes` into one end of a socketpair on a thread while `receive` reads the other
 *                               end on the caller's thread
 */

#ifndef GI_LOOPBACK_H
#define GI_LOOPBACK_H

#include "gi_testkit.h"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

namespace gitest {

inline Bytes captureSend(const std::function<int(int)> &send, int &sendResult) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        perror("socketpair");
        exit(2);
    }
    std::thread sender([&]() {
        sendResult = send(fds[0]);
        shutdown(fds[0], SHUT_WR);
    });
    Bytes got;
    unsigned char buf[65536];
    ssize_t n;
    while ((n = recv(fds[1], buf, sizeof(buf), 0)) > 0) {
        got.insert(got.end(), buf, buf + n);
    }
    sender.join();
    close(fds[0]);
    close(fds[1]);
    return got;
}

inline int replayInto(const Bytes &bytes, const std::function<int(int)> &receive) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        perror("socketpair");
        exit(2);
    }
    std::thread writer([&]() {
        size_t done = 0;
        while (done < bytes.size()) {
            const ssize_t n = send(fds[0], bytes.data() + done, bytes.size() - done, 0);
            if (n <= 0) {
                break;
            }
            done += (size_t)n;
        }
        shutdown(fds[0], SHUT_WR);
    });
    const int result = receive(fds[1]);
    writer.join();
    close(fds[0]);
    close(fds[1]);
    return result;
}

} // namespace gitest

#endif // GI_LOOPBACK_H
