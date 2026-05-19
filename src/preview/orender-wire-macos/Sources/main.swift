import AppKit
import Foundation

// ─── Command-line argument handling ──────────────────────────────────────────

let args = CommandLine.arguments

if args.contains("--help") || args.contains("-h") {
    print("""
    Usage: orender-wire <scene.rib>

    Open a RIB scene file in the interactive wireframe viewer.

    Controls:
      Left drag      Orbit
      Scroll / Pinch Zoom
      Middle drag    Pan
      R / Home       Reset to RIB camera
      S              Save camera to RIB file
      ⌘Q             Quit

    Exit codes:
      0  Success
      1  Usage error (bad arguments)
      2  File not found or not readable
      3  RIB parse failed
    """)
    exit(0)
}

if args.contains("--version") {
    print("orender-wire 1.0")
    exit(0)
}

guard args.count >= 2 else {
    fputs("usage: orender-wire <scene.rib>\n", stderr)
    exit(1)   // exit 1: usage error (missing argument)
}

let ribPath = args[1]
guard FileManager.default.fileExists(atPath: ribPath) else {
    fputs("orender-wire: cannot open '\(ribPath)': No such file or directory\n", stderr)
    exit(2)
}

// ─── Application startup ─────────────────────────────────────────────────────

let app = NSApplication.shared
app.setActivationPolicy(.regular)

let delegate = AppDelegate(ribPath: ribPath)
app.delegate = delegate

app.run()
