/**
 * SocketServer.swift — POSIX Unix socket listener.
 *
 * Binds to the socket path from argv[1] and loops accepting connections —
 * one per render session. After DONE the server socket stays open so orender
 * can reconnect for the next render without spawning a new helper process.
 * Reads TLV packets on a background DispatchQueue and dispatches parsed
 * events to the main queue via callbacks.
 */

import Foundation
import AppKit

public final class SocketServer: @unchecked Sendable {

    public let socketPath: String
    private var serverFd:  Int32 = -1
    private var clientFd:  Int32 = -1

    // Callbacks — all dispatched to main queue before invocation
    public var onStart:     ((StartPayload) -> Void)?
    public var onData:      ((DataPayload)  -> Void)?
    public var onDone:      ((DonePayload)  -> Void)?
    public var onInterrupt: (() -> Void)?
    public var onEmpty:     (() -> Void)?

    private var quitSent = false

    public init(socketPath: String) {
        self.socketPath = socketPath
    }

    // ---------------------------------------------------------------------------
    // Start (called from applicationDidFinishLaunching on main thread)
    // ---------------------------------------------------------------------------

    public func start() {
        serverFd = socket(AF_UNIX, SOCK_STREAM, 0)
        guard serverFd >= 0 else {
            fputs("orender-fb-macos: socket() failed: \(errno)\n", stderr)
            return
        }

        // Remove any stale socket file so bind() doesn't get EADDRINUSE.
        // This only runs at startup; reconnecting renders use the already-bound
        // server socket via the outer accept() loop, never re-binding.
        unlink(socketPath)

        var addr = sockaddr_un()
        addr.sun_family = sa_family_t(AF_UNIX)
        withUnsafeMutablePointer(to: &addr.sun_path) { ptr in
            socketPath.withCString { src in
                _ = strcpy(UnsafeMutableRawPointer(ptr)
                               .assumingMemoryBound(to: CChar.self), src)
            }
        }
        let addrSize = socklen_t(MemoryLayout<sockaddr_un>.size)
        let rc = withUnsafePointer(to: &addr) { ptr in
            ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Foundation.bind(serverFd, $0, addrSize)
            }
        }
        guard rc == 0 else {
            fputs("orender-fb-macos: bind() failed: \(errno)\n", stderr)
            return
        }
        listen(serverFd, 1)

        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            self?.acceptAndServe()
        }
    }

    // ---------------------------------------------------------------------------
    // Send QUIT packet to driver (called from AppDelegate.windowWillClose)
    // ---------------------------------------------------------------------------

    public func sendQuit() {
        guard clientFd >= 0, !quitSent else { return }
        quitSent = true
        var hdr: [UInt8] = [0x04, 0x00, 0x00, 0x00, 0x00]
        _ = Darwin.write(clientFd, &hdr, hdr.count)
    }

    // ---------------------------------------------------------------------------
    // Accept + TLV read loop (runs on background queue)
    //
    // Outer loop: accept one connection per render session. After DONE the
    // server socket stays open so orender can reconnect for the next render
    // without spawning a new helper process. QUIT closes the server socket
    // and lets the main-queue terminate() call exit the app.
    // ---------------------------------------------------------------------------

    private func acceptAndServe() {
        outerLoop: while true {
            clientFd = accept(serverFd, nil, nil)
            guard clientFd >= 0 else { return }
            // Keep serverFd open — we go back to accept() after each render.

            var tileCount    = 0
            var receivedQuit = false
            var continueAfterSession = false

            innerLoop: while true {
                var hdrBytes = [UInt8](repeating: 0, count: 5)
                guard readAll(clientFd, &hdrBytes, 5) else {
                    continueAfterSession = handleDisconnect(tileCount: tileCount,
                                                            receivedQuit: receivedQuit)
                    break innerLoop
                }

                guard let hdr = try? TLVParser.parseHeader(from: Data(hdrBytes)) else {
                    _ = handleDisconnect(tileCount: tileCount, receivedQuit: receivedQuit)
                    break innerLoop
                }

                var payload = [UInt8](repeating: 0, count: Int(hdr.length))
                if hdr.length > 0 {
                    guard readAll(clientFd, &payload, Int(hdr.length)) else {
                        continueAfterSession = handleDisconnect(tileCount: tileCount,
                                                                receivedQuit: receivedQuit)
                        break innerLoop
                    }
                }
                let payloadData = Data(payload)

                switch hdr.opcode {
                case .start:
                    guard let sp = try? TLVParser.parseStartPayload(from: payloadData) else {
                        fputs("orender-fb-macos: malformed START — exiting\n", stderr)
                        break innerLoop
                    }
                    let cb = self.onStart
                    DispatchQueue.main.async { cb?(sp) }

                case .data:
                    guard let dp = try? TLVParser.parseDataPayload(from: payloadData) else {
                        fputs("orender-fb-macos: malformed DATA — skipping\n", stderr)
                        break
                    }
                    tileCount += 1
                    let cb = self.onData
                    let tile = dp
                    DispatchQueue.main.async { cb?(tile) }

                case .done:
                    let dp = (try? TLVParser.parseDonePayload(from: payloadData))
                        ?? DonePayload(durationMillis: 0)
                    let cb = self.onDone
                    DispatchQueue.main.async { cb?(dp) }
                    continueAfterSession = true  // loop back to accept() for next render
                    break innerLoop

                case .quit:
                    receivedQuit = true
                    // Close server socket so no new connections land after we exit.
                    Darwin.close(serverFd)
                    serverFd = -1
                    DispatchQueue.main.async {
                        NSApplication.shared.terminate(nil)
                    }
                    continueAfterSession = false
                    break innerLoop
                }
            }

            Darwin.close(clientFd)
            clientFd = -1

            if !continueAfterSession { break outerLoop }
            // Continue outer loop → back to accept() for the next render session.
        }
    }

    // Returns true if the accept loop should continue (re-accept for next render).
    @discardableResult
    private func handleDisconnect(tileCount: Int, receivedQuit: Bool) -> Bool {
        guard !receivedQuit else { return false }
        if tileCount > 0 {
            let cb = self.onInterrupt
            DispatchQueue.main.async { cb?() }
            return true   // show interrupted state; wait for next render
        } else {
            Darwin.close(serverFd)
            serverFd = -1
            let cb = self.onEmpty
            DispatchQueue.main.async {
                cb?()
                NSApplication.shared.terminate(nil)
            }
            return false  // no tiles received; nothing to show — exit
        }
    }

    // ---------------------------------------------------------------------------
    // POSIX read — retries on EINTR
    // ---------------------------------------------------------------------------

    private func readAll(_ fd: Int32, _ buf: inout [UInt8], _ count: Int) -> Bool {
        var remaining = count
        var offset    = 0
        while remaining > 0 {
            let n = Darwin.read(fd, &buf[offset], remaining)
            if n <= 0 {
                if n < 0 && errno == EINTR { continue }
                return false
            }
            offset    += n
            remaining -= n
        }
        return true
    }
}
