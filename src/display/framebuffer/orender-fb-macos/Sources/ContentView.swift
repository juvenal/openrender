/**
 * ContentView.swift — SwiftUI image display view.
 *
 * Observes ImageStore and renders the current CGImage (or a dark placeholder
 * while waiting for the first tile).
 */

import SwiftUI
import AppKit

struct ContentView: View {
    @ObservedObject var store: ImageStore

    var body: some View {
        Group {
            if let img = store.cgImage {
                Image(decorative: img, scale: 1.0)
                    .resizable()
                    .aspectRatio(contentMode: .fit)
            } else {
                // Checkerboard placeholder before first tile arrives
                ZStack {
                    Color(white: 0.15)
                    Text("Waiting for render…")
                        .foregroundColor(.gray)
                        .font(.system(size: 14, weight: .regular, design: .monospaced))
                }
            }
        }
    }
}
