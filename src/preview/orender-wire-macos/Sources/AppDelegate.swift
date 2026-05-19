import AppKit
import Metal
import CRibPreview

// Swift 6 requires Sendable to cross actor boundaries; UnsafeMutablePointer<PreviewSceneC>
// is safe to pass since it's allocated on the heap and ownership is transferred.
private struct SendableScenePtr: @unchecked Sendable {
    let ptr: UnsafeMutablePointer<PreviewSceneC>?
}

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {

    private let ribPath: String
    private var window: NSWindow!
    private var spinner: NSProgressIndicator!

    init(ribPath: String) {
        self.ribPath = ribPath
        super.init()
    }

    // MARK: – NSApplicationDelegate

    func applicationDidFinishLaunching(_ notification: Notification) {
        buildMenu()
        openWindow()
        startLoading()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }

    func applicationWillTerminate(_ notification: Notification) {
        // ribpreview_free() is called immediately after WireframeRenderer is built;
        // there is nothing left to release here. ARC handles the MTLBuffer / MTLDevice.
    }

    // MARK: – Window

    private func openWindow() {
        let rect = NSRect(x: 100, y: 100, width: 800, height: 600)
        let style: NSWindow.StyleMask = [.titled, .closable, .miniaturizable, .resizable]
        window = NSWindow(contentRect: rect,
                          styleMask: style,
                          backing: .buffered,
                          defer: false)
        window.title = URL(fileURLWithPath: ribPath).lastPathComponent
        window.delegate = self
        window.isReleasedWhenClosed = false

        // Loading spinner centered in window
        spinner = NSProgressIndicator(frame: NSRect(x: 375, y: 275, width: 50, height: 50))
        spinner.style = .spinning
        spinner.isIndeterminate = true
        spinner.startAnimation(nil)

        let background = NSView(frame: rect)
        background.wantsLayer = true
        background.layer?.backgroundColor = NSColor(calibratedRed: 0.1, green: 0.1,
                                                     blue: 0.1, alpha: 1.0).cgColor
        background.addSubview(spinner)
        window.contentView = background

        window.makeKeyAndOrderFront(nil)
    }

    // MARK: – Background loading

    private func startLoading() {
        let path = ribPath
        Task {
            let wrapper = await Task.detached(priority: .userInitiated) {
                SendableScenePtr(ptr: ribpreview_load(path))
            }.value
            self.didLoad(scene: wrapper.ptr)
        }
    }

    private func didLoad(scene: UnsafeMutablePointer<PreviewSceneC>?) {
        spinner.stopAnimation(nil)

        guard let scene else {
            fputs("orender-wire: error: RIB parse failed (see above)\n", stderr)
            exit(3)   // exit 3: parse error
        }

        guard let metalDevice = MTLCreateSystemDefaultDevice() else {
            fputs("orender-wire: error: Metal is not available on this system\n", stderr)
            ribpreview_free(scene)
            NSApplication.shared.terminate(nil)
            return
        }

        let renderer = WireframeRenderer(metalDevice: metalDevice, scene: scene)
        ribpreview_free(scene)   // GPU buffer built; C heap copy no longer needed

        window.contentView = renderer
        window.makeFirstResponder(renderer)
        renderer.setNeedsDisplay(renderer.bounds)
    }

    // MARK: – Menu

    private func buildMenu() {
        let appName = "orender-wire"
        let mainMenu = NSMenu()

        let appItem = NSMenuItem(title: appName, action: nil, keyEquivalent: "")
        mainMenu.addItem(appItem)
        let appMenu = NSMenu(title: appName)
        appMenu.addItem(withTitle: "About \(appName)",
                        action: #selector(showAbout),
                        keyEquivalent: "")
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Quit \(appName)",
                        action: #selector(NSApplication.terminate(_:)),
                        keyEquivalent: "q")
        appItem.submenu = appMenu

        NSApp.mainMenu = mainMenu
    }

    @objc private func showAbout() {
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

    // MARK: – NSWindowDelegate

    func windowWillClose(_ notification: Notification) {
        NSApp.terminate(nil)
    }
}
