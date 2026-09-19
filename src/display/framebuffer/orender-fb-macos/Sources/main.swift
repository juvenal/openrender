/**
 * main.swift — orender-fb-macos entry point.
 *
 * Usage: orender-fb-macos <socket-path>
 *
 * Starts NSApplication, wires AppDelegate and SocketServer, and runs the
 * main loop. The window appears on receipt of the START packet. The process
 * remains alive (window open) after the renderer exits.
 */

import AppKit

guard CommandLine.arguments.count >= 2 else {
    fputs("orender-fb-macos: usage: orender-fb-macos <socket-path>\n", stderr)
    exit(1)
}
let socketPath = CommandLine.arguments[1]

ProcessInfo.processInfo.processName = "openRender"
let app      = NSApplication.shared
// Start as .accessory so the helper launches silently without stealing focus.
// AppDelegate switches to .regular when the first render window appears, which
// is when the Dock icon and menu bar should become visible.
app.setActivationPolicy(.accessory)

let server   = SocketServer(socketPath: socketPath)
let delegate = AppDelegate()
delegate.server = server
app.delegate    = delegate

app.run()
