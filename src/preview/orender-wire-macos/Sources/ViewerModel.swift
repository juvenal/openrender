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

    // ─── Data-document interactive state (User Story 2) ─────────────────────
    // Mirrors WireframeRenderer.lastDataSnapshot into @Published storage -- WireframeRenderer
    // itself isn't an ObservableObject, so SwiftUI's Commands menu and DocumentView's status
    // overlay observe these instead, refreshed after every sendDataKey() and on load.
    @Published private(set) var dataDocumentType: RibDataType?
    @Published private(set) var dataNumChannels: Int32 = 0
    @Published private(set) var dataChannelName: String?
    @Published private(set) var dataDetailLevel: Int32 = -1
    @Published private(set) var dataDrawMode: Int32 = 0

    var isDataDocumentOpen: Bool { dataDocumentType != nil }
    var hasChannels: Bool { dataNumChannels > 0 }
    var supportsDetailLevel: Bool { dataDocumentType == RIBDATA_TYPE_BRICKMAP }
    var supportsBoxDrawMode: Bool { dataDocumentType == RIBDATA_TYPE_BRICKMAP }
    var supportsDrawModeToggle: Bool {
        dataDocumentType == RIBDATA_TYPE_BRICKMAP || dataDocumentType == RIBDATA_TYPE_POINTCLOUD
    }

    // On-screen indicator (FR-016): channel/detail/draw-mode state, never only in a terminal.
    // nil for a RIB document or before a data document finishes loading -- DocumentView hides
    // the status overlay entirely in that case.
    var dataStatusText: String? {
        guard isDataDocumentOpen else { return nil }
        var parts: [String] = []
        if hasChannels, let name = dataChannelName {
            parts.append("Channel: \(name)")
        }
        if supportsDetailLevel {
            parts.append("Detail: \(dataDetailLevel)")
        }
        parts.append("Draw: \(drawModeDisplayName)")
        return parts.joined(separator: "   •   ")
    }

    private var drawModeDisplayName: String {
        switch dataDocumentType {
        case RIBDATA_TYPE_BRICKMAP:
            switch dataDrawMode {
            case 0: return "Boxes"
            case 1: return "Discs"
            default: return "Points"
            }
        case RIBDATA_TYPE_POINTCLOUD:
            return dataDrawMode == 1 ? "Discs" : "Points"
        default:
            return "Fixed"
        }
    }

    // Applies one legacy key (`m l b d p q w`) to the open data document and refreshes the
    // published state above. No-op for a RIB document or a key the document type doesn't
    // recognize (see WireframeRenderer.sendDataKey).
    func sendDataKey(_ key: Character) {
        guard let r = renderer, r.sendDataKey(key) else { return }
        refreshDataState()
    }

    private func refreshDataState() {
        guard let snap = renderer?.lastDataSnapshot else {
            dataDocumentType = nil
            dataNumChannels  = 0
            dataChannelName  = nil
            dataDetailLevel  = -1
            dataDrawMode     = 0
            return
        }
        dataDocumentType = snap.documentType
        dataNumChannels  = snap.numChannels
        dataChannelName  = renderer?.dataChannelName
        dataDetailLevel  = snap.detailLevel
        dataDrawMode     = snap.drawMode
    }

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
        refreshDataState()
    }
}
