import AppKit
import Darwin
import Foundation
import CRibPreview

// ─── Command-line argument handling ──────────────────────────────────────────
//
// Delegates entirely to wireCliRun() (src/preview/libribpreview/wireCli.cpp) -- the shared,
// already-tested (test_wire_cli.cpp) argument grammar, --help/--version text, --json headless
// mode, and exit codes 1-4, so this frontend and the Linux one never drift from each other or
// from cli-interface.md. This replaces main.swift's own former ad-hoc --help/--version/argc
// checks, which never supported --json or --type at all.
//
// Must run, and exit() on WIRE_CLI_EXIT, before the ORENDER_WIRE_GUI re-exec check below --
// otherwise `--json` would detach into a background GUI process and a test harness (or an SSH
// session with no window server) would see exit 0 with no output instead of the JSON/text on
// this process's own stdout.

var outPathPtr: UnsafeMutablePointer<CChar>?
var wireExitCode: Int32 = 0
let wireAction = wireCliRun(CommandLine.argc, CommandLine.unsafeArgv, &outPathPtr, &wireExitCode)

if wireAction == WIRE_CLI_EXIT {
    exit(wireExitCode)
}

guard let outPathPtr else {
    fputs("orender-wire: internal error: wireCliRun did not return a path to open\n", stderr)
    exit(1)
}
let ribPath = String(cString: outPathPtr)
free(outPathPtr)

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
