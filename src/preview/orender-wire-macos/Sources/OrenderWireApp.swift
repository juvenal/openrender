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
            // Data-document controls carried over from the deleted oshow tool (User Story 2):
            // channel/detail-level/draw-mode, as real menu items with the same legacy letter
            // keyboard shortcuts, instead of only being reachable via a terminal-invisible key
            // press. Each item is enabled only when the open document type actually supports it
            // (FR-017 for channels; brick maps have three draw modes, point clouds have two, and
            // 'd'/'p' are shared between them without conflict since only one document is ever
            // open at a time).
            CommandMenu("Data") {
                Button("Previous Channel") { model.sendDataKey("q") }
                    .keyboardShortcut("q", modifiers: [])
                    .disabled(!model.hasChannels)
                Button("Next Channel") { model.sendDataKey("w") }
                    .keyboardShortcut("w", modifiers: [])
                    .disabled(!model.hasChannels)

                Divider()

                Button("Increase Detail Level") { model.sendDataKey("m") }
                    .keyboardShortcut("m", modifiers: [])
                    .disabled(!model.supportsDetailLevel)
                Button("Decrease Detail Level") { model.sendDataKey("l") }
                    .keyboardShortcut("l", modifiers: [])
                    .disabled(!model.supportsDetailLevel)

                Divider()

                Button("Draw as Boxes") { model.sendDataKey("b") }
                    .keyboardShortcut("b", modifiers: [])
                    .disabled(!model.supportsBoxDrawMode)
                Button("Draw as Discs") { model.sendDataKey("d") }
                    .keyboardShortcut("d", modifiers: [])
                    .disabled(!model.supportsDrawModeToggle)
                Button("Draw as Points") { model.sendDataKey("p") }
                    .keyboardShortcut("p", modifiers: [])
                    .disabled(!model.supportsDrawModeToggle)
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
