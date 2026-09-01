import SwiftUI
import AppKit

// Small delegate whose only job is to reproduce AppDelegate's former
// applicationShouldTerminateAfterLastWindowClosed(_:) — SwiftUI's WindowGroup has no direct
// equivalent for this on macOS 12, so a minimal NSApplicationDelegateAdaptor supplies it.
final class TerminationDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }
}

struct OrenderWireApp: SwiftUI.App {
    @NSApplicationDelegateAdaptor(TerminationDelegate.self) var appDelegate
    @StateObject private var model = ViewerModel.shared

    var body: some Scene {
        WindowGroup {
            DocumentView(model: model)
        }
        .commands {
            CommandGroup(replacing: .appInfo) {
                Button("About orender-wire") {
                    showAbout()
                }
            }
        }
    }

    private func showAbout() {
        let creditsText = """
            Open Rendering Tools — RenderMan-compatible renderer.

            © Copyright 2025–2026 Juvenal A. Silva Jr.
            All rights reserved.

            The RenderMan® Interface Procedures and RIB Protocol are:
            Copyright 1988, 1989, Pixar. All rights reserved.
            RenderMan® is a registered trademark of Pixar.
            """
        let credits = NSAttributedString(
            string: creditsText,
            attributes: [.font: NSFont.systemFont(ofSize: NSFont.smallSystemFontSize)]
        )
        NSApp.orderFrontStandardAboutPanel(options: [
            .applicationName:    "orender-wire" as NSString,
            .applicationVersion: "1.0.0" as NSString,
            .credits:            credits,
        ])
    }
}
