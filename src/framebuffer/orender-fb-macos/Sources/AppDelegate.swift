/**
 * AppDelegate.swift — NSApplication delegate.
 *
 * Creates the HUD-style NSPanel on receipt of the START packet (so the window
 * has the correct dimensions and title). Provides a File menu with Save Image
 * (TIFF/PNG) and Quit — no other menus.
 */

import AppKit
import SwiftUI
import Combine
import UniformTypeIdentifiers

@MainActor
class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {

    var server: SocketServer!

    // Current render session's image store (updated on each START).
    private var store: ImageStore?

    // One entry per open window. Removing all entries triggers app exit.
    private struct Session {
        let panel:    NSPanel
        let store:    ImageStore
        let observer: AnyCancellable  // keeps panel title in sync with store
    }
    private var sessions: [Session] = []

    // ---------------------------------------------------------------------------
    // NSApplicationDelegate
    // ---------------------------------------------------------------------------

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Bundle.main is the .app bundle when running from Contents/MacOS/;
        // AppIcon.icns lives in Contents/Resources/ (also declared via CFBundleIconFile).
        if let iconURL = Bundle.main.url(forResource: "AppIcon", withExtension: "icns"),
           let icon = NSImage(contentsOf: iconURL) {
            NSApp.applicationIconImage = icon
        }
        buildMenu()

        // Wire SocketServer callbacks (all called on main queue by SocketServer)
        server.onStart = { [weak self] sp in
            self?.handleStart(sp)
        }
        server.onData = { [weak self] dp in
            self?.store?.applyTile(dp)
        }
        server.onDone = { [weak self] in
            self?.store?.markDone()
        }
        server.onInterrupt = { [weak self] in
            self?.store?.markInterrupted()
        }
        server.onEmpty = {
            // No tiles received — window never shown; app exits (handled by SocketServer)
        }

        server.start()
    }

    // ---------------------------------------------------------------------------
    // Window creation — called on every START packet (one new window per render)
    // ---------------------------------------------------------------------------

    private func handleStart(_ sp: StartPayload) {
        let s = ImageStore(width: sp.width, height: sp.height,
                           numSamples: sp.numSamples, title: sp.title)
        store = s

        let rect = NSRect(x: 100, y: 100,
                          width: CGFloat(sp.width), height: CGFloat(sp.height))
        let style: NSWindow.StyleMask = [.hudWindow, .closable, .titled, .resizable]
        let p = NSPanel(contentRect: rect,
                        styleMask: style,
                        backing: .buffered,
                        defer: false)
        p.isFloatingPanel      = false
        p.level                = .normal
        p.title                = s.windowTitle
        p.contentView          = NSHostingView(rootView: ContentView(store: s))
        p.delegate             = self
        p.isReleasedWhenClosed = false
        p.hidesOnDeactivate    = false  // stay visible when orender/terminal has focus

        // Switch from .accessory to .regular on the first window: this is when
        // the Dock icon and menu bar should appear — not at process launch.
        if sessions.isEmpty {
            NSApp.setActivationPolicy(.regular)
        }

        let obs = s.$windowTitle
            .receive(on: DispatchQueue.main)
            .sink { [weak p] newTitle in p?.title = newTitle }

        sessions.append(Session(panel: p, store: s, observer: obs))
        p.orderFrontRegardless()
    }

    // ---------------------------------------------------------------------------
    // NSWindowDelegate — track open windows; quit when the last one closes
    // ---------------------------------------------------------------------------

    func windowWillClose(_ notification: Notification) {
        guard let closing = notification.object as? NSPanel else { return }

        // If a render is currently streaming to this connection, notify orender.
        // sendQuit() is a no-op when no render is active (clientFd < 0).
        server.sendQuit()

        sessions.removeAll { $0.panel === closing }

        if sessions.isEmpty {
            NSApp.terminate(nil)
        }
    }

    // ---------------------------------------------------------------------------
    // Menu
    // ---------------------------------------------------------------------------

    private func buildMenu() {
        let appName = Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String
                      ?? "openRender"

        let mainMenu = NSMenu()

        let appItem = NSMenuItem(title: appName, action: nil, keyEquivalent: "")
        mainMenu.addItem(appItem)
        let appMenu = NSMenu(title: appName)
        appMenu.addItem(withTitle: "About \(appName)",
                        action: #selector(showAbout(_:)),
                        keyEquivalent: "")
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Quit \(appName)",
                        action: #selector(NSApplication.terminate(_:)),
                        keyEquivalent: "q")
        appItem.submenu = appMenu

        let fileItem = NSMenuItem()
        mainMenu.addItem(fileItem)
        let fileMenu = NSMenu(title: "File")
        let saveItem = NSMenuItem(title: "Save Image…",
                                  action: #selector(saveImage(_:)),
                                  keyEquivalent: "s")
        saveItem.keyEquivalentModifierMask = .command
        fileMenu.addItem(saveItem)
        fileItem.submenu = fileMenu

        NSApp.mainMenu = mainMenu
    }

    // ---------------------------------------------------------------------------
    // About openRender
    // ---------------------------------------------------------------------------

    @objc private func showAbout(_ sender: Any?) {
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
        let appName = Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String
                      ?? "openRender"
        NSApp.orderFrontStandardAboutPanel(options: [
            .applicationName:    appName as NSString,
            .applicationVersion: "1.0.0" as NSString,
            .credits:            credits
        ])
    }

    // ---------------------------------------------------------------------------
    // Save Image
    // ---------------------------------------------------------------------------

    @objc private func saveImage(_ sender: Any?) {
        // Resolve the store from the key window before the save panel takes focus.
        let activeStore: ImageStore?
        if let keyPanel = NSApp.keyWindow as? NSPanel,
           let session  = sessions.first(where: { $0.panel === keyPanel }) {
            activeStore = session.store
        } else {
            activeStore = store  // fallback: last render
        }
        guard let img = activeStore?.cgImage else { return }
        let savePanel = NSSavePanel()
        savePanel.title = "Save Framebuffer Image"
        savePanel.allowedContentTypes = [.tiff, .png]
        savePanel.nameFieldStringValue = "framebuffer.tiff"
        savePanel.begin { [weak self] response in
            guard response == .OK, let url = savePanel.url else { return }
            self?.writeCGImage(img, to: url)
        }
    }

    private func writeCGImage(_ image: CGImage, to url: URL) {
        let type: UTType = url.pathExtension.lowercased() == "png" ? .png : .tiff
        guard let dest = CGImageDestinationCreateWithURL(
            url as CFURL, type.identifier as CFString, 1, nil) else {
            fputs("orender-fb-macos: failed to create image destination\n", stderr)
            return
        }
        CGImageDestinationAddImage(dest, image, nil)
        if !CGImageDestinationFinalize(dest) {
            fputs("orender-fb-macos: failed to write image to \(url.path)\n", stderr)
        }
    }
}
