/**
 * Protocol.swift — TLV packet definitions and parser for the framebuffer IPC protocol.
 *
 * Wire format (matching fbipc.h):
 *   [opcode:UInt8][length:UInt32-LE][payload:length bytes]
 *
 * See: specs/004-macos-framebuffer-output/contracts/ipc-protocol.md
 */

import Foundation

// ---------------------------------------------------------------------------
// Opcode
// ---------------------------------------------------------------------------

public enum FBOpcode: UInt8, Sendable {
    case start = 0x01
    case data  = 0x02
    case done  = 0x03
    case quit  = 0x04
}

// ---------------------------------------------------------------------------
// Parsed packet types
// ---------------------------------------------------------------------------

public struct PacketHeader: Sendable {
    public let opcode: FBOpcode
    public let length: UInt32
}

public struct StartPayload: Sendable {
    public let width:      UInt32
    public let height:     UInt32
    public let numSamples: UInt32
    public let title:      String
}

public struct DataPayload: Sendable {
    public let x:      UInt32
    public let y:      UInt32
    public let w:      UInt32
    public let h:      UInt32
    public let pixels: [Float]
}

// ---------------------------------------------------------------------------
// Parser errors
// ---------------------------------------------------------------------------

public enum TLVError: Error {
    case truncated(expected: Int, got: Int)
    case unknownOpcode(UInt8)
    case titleDecodingFailed
}

// ---------------------------------------------------------------------------
// TLVParser — stateless, operates on Data blobs
// ---------------------------------------------------------------------------

public enum TLVParser {

    // Parse a 5-byte header from Data. Throws TLVError.truncated or unknownOpcode.
    public static func parseHeader(from data: Data) throws -> PacketHeader {
        guard data.count >= 5 else {
            throw TLVError.truncated(expected: 5, got: data.count)
        }
        let rawOpcode = data[data.startIndex]
        guard let opcode = FBOpcode(rawValue: rawOpcode) else {
            throw TLVError.unknownOpcode(rawOpcode)
        }
        let length = data.dropFirst(1).prefix(4).withUnsafeBytes { ptr in
            ptr.loadUnaligned(as: UInt32.self).littleEndian
        }
        return PacketHeader(opcode: opcode, length: length)
    }

    // Parse a START payload. Data must include the 16-byte fixed fields + title bytes.
    public static func parseStartPayload(from data: Data) throws -> StartPayload {
        let fixedSize = 16
        guard data.count >= fixedSize else {
            throw TLVError.truncated(expected: fixedSize, got: data.count)
        }
        func u32(at offset: Int) -> UInt32 {
            data.dropFirst(offset).prefix(4).withUnsafeBytes { ptr in
                ptr.loadUnaligned(as: UInt32.self).littleEndian
            }
        }
        let width      = u32(at: 0)
        let height     = u32(at: 4)
        let numSamples = u32(at: 8)
        let titleLen   = u32(at: 12)

        let totalExpected = fixedSize + Int(titleLen)
        guard data.count >= totalExpected else {
            throw TLVError.truncated(expected: totalExpected, got: data.count)
        }
        let titleData = data.dropFirst(fixedSize).prefix(Int(titleLen))
        guard let title = String(bytes: titleData, encoding: .utf8) else {
            throw TLVError.titleDecodingFailed
        }
        return StartPayload(width: width, height: height,
                            numSamples: numSamples, title: title)
    }

    // Parse a DATA payload. Data must include the 16-byte fixed fields + pixel floats.
    public static func parseDataPayload(from data: Data) throws -> DataPayload {
        let fixedSize = 16
        guard data.count >= fixedSize else {
            throw TLVError.truncated(expected: fixedSize, got: data.count)
        }
        func u32(at offset: Int) -> UInt32 {
            data.dropFirst(offset).prefix(4).withUnsafeBytes { ptr in
                ptr.loadUnaligned(as: UInt32.self).littleEndian
            }
        }
        let x = u32(at: 0)
        let y = u32(at: 4)
        let w = u32(at: 8)
        let h = u32(at: 12)

        let pixelBytes = data.dropFirst(fixedSize)
        let floatCount = pixelBytes.count / MemoryLayout<Float>.size
        var pixels = [Float](repeating: 0, count: floatCount)
        _ = pixels.withUnsafeMutableBytes { dst in
            pixelBytes.copyBytes(to: dst)
        }
        // Convert from little-endian if needed (Float bytes are already LE on Apple Silicon / x86)
        return DataPayload(x: x, y: y, w: w, h: h, pixels: pixels)
    }
}
