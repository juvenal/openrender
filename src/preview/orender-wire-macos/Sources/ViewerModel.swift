import AppKit
import MetalKit
import CRibPreview

// Swift 6 requires Sendable to cross actor boundaries; UnsafeMutablePointer<PreviewSceneC>
// is safe to pass since it's allocated on the heap and ownership is transferred.
private struct SendableScenePtr: @unchecked Sendable {
    let ptr: UnsafeMutablePointer<PreviewSceneC>?
}

private struct SendableDataDoc: @unchecked Sendable {
    let ptr: OpaquePointer?
}

// Holds the currently open document's state and bridges it to SwiftUI.
// Constructed once in main.swift, before OrenderWireApp.main() hands off to the SwiftUI
// lifecycle, and referenced via `ViewerModel.shared` — this is what lets main.swift's
// argument parsing (which already knows ribPath) stay unmodified.
@MainActor
final class ViewerModel: ObservableObject {

    static var shared: ViewerModel!

    let ribPath: String
    @Published private(set) var renderer: WireframeRenderer?

    var windowTitle: String {
        URL(fileURLWithPath: ribPath).lastPathComponent
    }

    init(ribPath: String) {
        self.ribPath = ribPath
    }

    // Mirrors AppDelegate's former startLoading()/didLoad(scene:) exactly.
    func load() async {
        guard renderer == nil else { return }

        // Content-based detection (FR-001): -1 means no data-file magic at all (try RIB); any
        // other value -- including -2, "recognized magic but incompatible version/word-size" --
        // means this IS a data file and must be routed to ribdata_open(), which will report the
        // real failure reason, rather than falling back to RIB and silently rendering an empty
        // scene. Mirrors wireCliRun()'s auto-detection in src/preview/libribpreview/wireCli.cpp.
        if ribdata_sniff(ribPath) != -1 {
            await loadDataDocument()
        } else {
            await loadRibScene()
        }
    }

    private func loadRibScene() async {
        let path = ribPath
        let wrapper = await Task.detached(priority: .userInitiated) {
            SendableScenePtr(ptr: ribpreview_load(path))
        }.value

        guard let scene = wrapper.ptr else {
            fputs("orender-wire: error: RIB parse failed (see above)\n", stderr)
            exit(3)   // exit 3: parse error
        }

        guard let metalDevice = MTLCreateSystemDefaultDevice() else {
            fputs("orender-wire: error: Metal is not available on this system\n", stderr)
            ribpreview_free(scene)
            NSApplication.shared.terminate(nil)
            return
        }

        let r = WireframeRenderer(metalDevice: metalDevice, scene: scene)
        ribpreview_free(scene)   // GPU buffer built; C heap copy no longer needed
        renderer = r
    }

    private func loadDataDocument() async {
        let path = ribPath
        let wrapper = await Task.detached(priority: .userInitiated) {
            var err: Int32 = 0
            return SendableDataDoc(ptr: ribdata_open(path, &err))
        }.value

        guard let doc = wrapper.ptr else {
            fputs("orender-wire: error: \(path) is not a recognized data file\n", stderr)
            exit(4)   // exit 4: data file rejected (bad type/version/word-size)
        }

        guard let metalDevice = MTLCreateSystemDefaultDevice() else {
            fputs("orender-wire: error: Metal is not available on this system\n", stderr)
            ribdata_close(doc)
            NSApplication.shared.terminate(nil)
            return
        }

        renderer = WireframeRenderer(metalDevice: metalDevice, dataDoc: doc)
    }
}
