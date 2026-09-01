import SwiftUI
import MetalKit

// NSViewRepresentable wrapping the existing WireframeRenderer MTKView unchanged.
//
// updateNSView re-asserts first responder whenever SwiftUI's view hierarchy isn't already
// giving it focus. A SwiftUI-hosted NSViewRepresentable is frequently NOT made first responder
// automatically (unlike the old AppDelegate, which set it as `window.contentView` directly) —
// without this, every keyboard shortcut (orbit reset, save camera) would silently stop working
// while the app still renders correctly.
struct MetalRendererView: NSViewRepresentable {
    let renderer: WireframeRenderer

    func makeNSView(context: Context) -> WireframeRenderer {
        renderer
    }

    func updateNSView(_ nsView: WireframeRenderer, context: Context) {
        if nsView.window?.firstResponder !== nsView {
            nsView.window?.makeFirstResponder(nsView)
        }
        // WireframeRenderer is isPaused/enableSetNeedsDisplay (no internal display link — see
        // its own doc comment), so it only ever draws in response to an explicit request. The
        // AppKit-era AppDelegate.didLoad() supplied that first request directly after attaching
        // the view (`renderer.setNeedsDisplay(renderer.bounds)`); under SwiftUI hosting there is
        // no equivalent one-shot attachment point, so request it here instead, on every layout
        // pass — idempotent, and guaranteed to eventually land after AppKit assigns real bounds.
        nsView.setNeedsDisplay(nsView.bounds)
    }
}

struct DocumentView: View {
    @ObservedObject var model: ViewerModel

    var body: some View {
        ZStack {
            Color(red: 0.1, green: 0.1, blue: 0.1)
                .ignoresSafeArea()
            if let renderer = model.renderer {
                MetalRendererView(renderer: renderer)
            } else {
                ProgressView()
                    .progressViewStyle(.circular)
                    .controlSize(.large)
            }
        }
        .frame(minWidth: 800, minHeight: 600)
        .navigationTitle(model.windowTitle)
        .task {
            await model.load()
        }
    }
}
