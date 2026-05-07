/**
 * ImageStore.swift — Observable image buffer and session state.
 *
 * Created on receipt of the START packet with the render's exact dimensions.
 * Receives tile updates from SocketServer and drives ContentView redraws.
 * All mutations must occur on the main actor.
 */

import AppKit
import Foundation
import OSLog

// Mirrors the ORENDER_LOG_LEVEL value set by orender -x <level>.
// The helper inherits the env var from orender via posix_spawn.
private let fbDebugEnabled: Bool = {
    guard let v = ProcessInfo.processInfo.environment["ORENDER_LOG_LEVEL"],
          let n = Int(v) else { return false }
    return n >= 4
}()

private func fbDebug(_ message: @autoclosure () -> String) {
    guard fbDebugEnabled else { return }
    fputs("orender-fb-macos [DEBUG]: \(message())\n", stderr)
}

@MainActor
public final class ImageStore: ObservableObject {

    // ---------------------------------------------------------------------------
    // Published state
    // ---------------------------------------------------------------------------

    @Published public private(set) var cgImage:     CGImage?
    @Published public private(set) var windowTitle: String

    // ---------------------------------------------------------------------------
    // Private state
    // ---------------------------------------------------------------------------

    public let width:      Int
    public let height:     Int
    public let numSamples: Int
    private let baseTitle: String
    private var context:   CGContext?
    private var interrupted = false
    private var completed   = false

    // ---------------------------------------------------------------------------
    // Init (called once START packet is received)
    // ---------------------------------------------------------------------------

    public init(width: UInt32, height: UInt32, numSamples: UInt32, title: String) {
        self.width      = Int(width)
        self.height     = Int(height)
        self.numSamples = Int(numSamples)
        self.baseTitle  = title.isEmpty ? "orender" : title
        self.windowTitle = "orender — \(self.baseTitle)"

        // Create RGBA8 backing context
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
        self.context = CGContext(
            data: nil,
            width: Int(width),
            height: Int(height),
            bitsPerComponent: 8,
            bytesPerRow: Int(width) * 4,
            space: colorSpace,
            bitmapInfo: bitmapInfo.rawValue
        )
        // Checkerboard placeholder
        if let ctx = self.context {
            ctx.setFillColor(CGColor(gray: 0.2, alpha: 1.0))
            ctx.fill(CGRect(x: 0, y: 0, width: Int(width), height: Int(height)))
        }
    }

    // ---------------------------------------------------------------------------
    // Tile application (FIFO — called in order by SocketServer)
    // ---------------------------------------------------------------------------

    public func applyTile(_ tile: DataPayload) {
        fbDebug("applyTile x=\(tile.x) y=\(tile.y) w=\(tile.w) h=\(tile.h) context=\(context != nil) interrupted=\(interrupted) completed=\(completed)")
        guard !interrupted, !completed else { return }
        guard let ctx = context else {
            fbDebug("applyTile skipped — context is nil")
            return
        }

        let tileW = Int(tile.w)
        let tileH = Int(tile.h)
        let tileX = Int(tile.x)
        // CoreGraphics origin is bottom-left; protocol origin is top-left
        let tileY = height - Int(tile.y) - tileH

        // Build RGBA8 buffer from float pixels
        var rgba = [UInt8](repeating: 255, count: tileW * tileH * 4)
        for py in 0..<tileH {
            for px in 0..<tileW {
                let srcIdx = (py * tileW + px) * numSamples
                let dstIdx = (py * tileW + px) * 4
                rgba[dstIdx + 0] = floatToByte(tile.pixels[srcIdx + 0])
                rgba[dstIdx + 1] = floatToByte(tile.pixels[srcIdx + 1])
                rgba[dstIdx + 2] = floatToByte(tile.pixels[srcIdx + 2])
                rgba[dstIdx + 3] = numSamples == 4
                    ? floatToByte(tile.pixels[srcIdx + 3]) : 255
            }
        }

        let colorSpace = CGColorSpaceCreateDeviceRGB()
        let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
        rgba.withUnsafeMutableBytes { rawBuffer in
            guard let base = rawBuffer.baseAddress,
                  let provider = CGDataProvider(
                      data: Data(bytes: base, count: rawBuffer.count) as CFData),
                  let tileImage = CGImage(
                      width: tileW, height: tileH,
                      bitsPerComponent: 8, bitsPerPixel: 32,
                      bytesPerRow: tileW * 4,
                      space: colorSpace, bitmapInfo: bitmapInfo,
                      provider: provider, decode: nil,
                      shouldInterpolate: false, intent: .defaultIntent)
            else { return }
            ctx.draw(tileImage, in: CGRect(x: tileX, y: tileY,
                                           width: tileW, height: tileH))
        }
        cgImage = ctx.makeImage()
        if cgImage == nil { fbDebug("WARNING — ctx.makeImage() returned nil") }
    }

    // ---------------------------------------------------------------------------
    // Session lifecycle
    // ---------------------------------------------------------------------------

    public func markDone() {
        completed   = true
        windowTitle = "Rendering Complete — \(baseTitle)"
    }

    public func markInterrupted() {
        interrupted = true
        windowTitle = "Interrupted — \(baseTitle)"
    }

    // ---------------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------------

    private func floatToByte(_ f: Float) -> UInt8 {
        UInt8(max(0, min(255, Int(f * 255.0 + 0.5))))
    }
}
