import AppKit
import Darwin
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

// ─── Detach from the calling terminal ────────────────────────────────────────
//
// When invoked from the CLI, re-exec ourselves as an independent process so the
// shell prompt returns immediately.  The environment sentinel ORENDER_WIRE_GUI=1
// prevents the re-launched child from looping.  Runtime errors in the child are
// written to stderr, which it inherits from the parent and which appears both in
// the terminal and in macOS Console.app.
//
// This must NOT run when launched via Finder, the Dock, or `open` — those go through
// LaunchServices, whose launched process always has launchd (PID 1) as its immediate
// parent. Re-execing in that case orphans the Launch-Services-tracked process (its Dock
// icon disappears the instant it exits) while the untracked child survives headlessly,
// with no window and no way to bring it forward — indistinguishable from a crash. Skip
// the detach whenever the parent is launchd; there is no shell prompt to give back in
// that case anyway.

let launchedViaLaunchServices = getppid() == 1

if !launchedViaLaunchServices && ProcessInfo.processInfo.environment["ORENDER_WIRE_GUI"] == nil {
    guard let self_ = Bundle.main.executableURL else {
        fputs("orender-wire: cannot resolve executable path\n", stderr)
        exit(1)
    }
    let child = Process()
    child.executableURL = self_
    child.arguments     = Array(CommandLine.arguments.dropFirst())
    var env = ProcessInfo.processInfo.environment
    env["ORENDER_WIRE_GUI"] = "1"
    child.environment = env
    do {
        try child.run()
    } catch {
        fputs("orender-wire: failed to launch GUI process: \(error)\n", stderr)
        exit(1)
    }
    exit(0)   // parent: return control to the shell
}

// ─── Application startup ─────────────────────────────────────────────────────
//
// Bootstraps via SwiftUI's App lifecycle (OrenderWireApp.main() creates and runs
// NSApplication internally) rather than the AppKit NSApplication/NSApplicationDelegate
// pair this replaced. ViewerModel.shared is constructed here, before handing off, so
// OrenderWireApp can pick it up without main.swift needing to know anything about SwiftUI.

ViewerModel.shared = ViewerModel(ribPath: ribPath)
OrenderWireApp.main()
