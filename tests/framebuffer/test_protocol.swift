/**
 * tests/framebuffer/test_protocol.swift
 *
 * Unit tests for Protocol.swift — TLV packet parser.
 * Run from within the orender-fb-macos Swift package test target.
 */

import XCTest
import Foundation
@testable import OrenderfbmacosSources

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

private func makeHeader(opcode: UInt8, length: UInt32) -> Data {
    var data = Data(count: 5)
    data[0] = opcode
    var le = length.littleEndian
    withUnsafeBytes(of: &le) { raw in
        data.replaceSubrange(1..<5, with: raw)
    }
    return data
}

private func makeStartPayload(width: UInt32, height: UInt32,
                               numSamples: UInt32, title: String) -> Data {
    var data = Data(count: 16)
    func write32(_ v: UInt32, at offset: Int) {
        var le = v.littleEndian
        withUnsafeBytes(of: &le) { raw in
            data.replaceSubrange(offset..<offset+4, with: raw)
        }
    }
    write32(width,      at: 0)
    write32(height,     at: 4)
    write32(numSamples, at: 8)
    let titleBytes = Array(title.utf8)
    write32(UInt32(titleBytes.count), at: 12)
    data.append(contentsOf: titleBytes)
    return data
}

private func makeDataPayload(x: UInt32, y: UInt32, w: UInt32, h: UInt32,
                              numSamples: UInt32, pixels: [Float]) -> Data {
    var data = Data(count: 16)
    func write32(_ v: UInt32, at offset: Int) {
        var le = v.littleEndian
        withUnsafeBytes(of: &le) { raw in
            data.replaceSubrange(offset..<offset+4, with: raw)
        }
    }
    write32(x, at: 0)
    write32(y, at: 4)
    write32(w, at: 8)
    write32(h, at: 12)
    for f in pixels {
        var fle = f
        withUnsafeBytes(of: &fle) { raw in data.append(contentsOf: raw) }
    }
    return data
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

final class ProtocolTests: XCTestCase {

    // Opcode constants match protocol spec
    func testOpcodeValues() {
        XCTAssertEqual(FBOpcode.start.rawValue,  0x01)
        XCTAssertEqual(FBOpcode.data.rawValue,   0x02)
        XCTAssertEqual(FBOpcode.done.rawValue,   0x03)
        XCTAssertEqual(FBOpcode.quit.rawValue,   0x04)
    }

    // Header parsing — START opcode, length field
    func testParseStartHeader() throws {
        let hdrData = makeHeader(opcode: 0x01, length: 20)
        let hdr = try TLVParser.parseHeader(from: hdrData)
        XCTAssertEqual(hdr.opcode, .start)
        XCTAssertEqual(hdr.length, 20)
    }

    // Header parsing — DATA opcode
    func testParseDataHeader() throws {
        let hdrData = makeHeader(opcode: 0x02, length: 256)
        let hdr = try TLVParser.parseHeader(from: hdrData)
        XCTAssertEqual(hdr.opcode, .data)
        XCTAssertEqual(hdr.length, 256)
    }

    // Header parsing — DONE opcode, zero length
    func testParseDoneHeader() throws {
        let hdrData = makeHeader(opcode: 0x03, length: 0)
        let hdr = try TLVParser.parseHeader(from: hdrData)
        XCTAssertEqual(hdr.opcode, .done)
        XCTAssertEqual(hdr.length, 0)
    }

    // Header parsing — QUIT opcode, zero length
    func testParseQuitHeader() throws {
        let hdrData = makeHeader(opcode: 0x04, length: 0)
        let hdr = try TLVParser.parseHeader(from: hdrData)
        XCTAssertEqual(hdr.opcode, .quit)
        XCTAssertEqual(hdr.length, 0)
    }

    // Malformed header — unknown opcode
    func testMalformedOpcodeThrows() {
        let hdrData = makeHeader(opcode: 0xFF, length: 0)
        XCTAssertThrowsError(try TLVParser.parseHeader(from: hdrData))
    }

    // START payload — normal title
    func testParseStartPayload() throws {
        let payload = makeStartPayload(width: 1920, height: 1080,
                                        numSamples: 3, title: "test-scene")
        let sp = try TLVParser.parseStartPayload(from: payload)
        XCTAssertEqual(sp.width,      1920)
        XCTAssertEqual(sp.height,     1080)
        XCTAssertEqual(sp.numSamples, 3)
        XCTAssertEqual(sp.title,      "test-scene")
    }

    // START payload — zero-length title
    func testParseStartPayloadZeroTitle() throws {
        let payload = makeStartPayload(width: 320, height: 240, numSamples: 4, title: "")
        let sp = try TLVParser.parseStartPayload(from: payload)
        XCTAssertEqual(sp.title, "")
        XCTAssertEqual(sp.numSamples, 4)
    }

    // START payload — 512-byte title (max)
    func testParseStartPayloadMaxTitle() throws {
        let longTitle = String(repeating: "A", count: 512)
        let payload = makeStartPayload(width: 100, height: 100,
                                        numSamples: 3, title: longTitle)
        let sp = try TLVParser.parseStartPayload(from: payload)
        XCTAssertEqual(sp.title.count, 512)
    }

    // DATA payload — RGB tile
    func testParseDataPayload() throws {
        let pixels: [Float] = [1.0, 0.5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.5, 0.5, 0.5]
        // 2x2 tile, 3 samples = 12 floats
        let payload = makeDataPayload(x: 10, y: 20, w: 2, h: 2,
                                       numSamples: 3, pixels: pixels)
        let dp = try TLVParser.parseDataPayload(from: payload)
        XCTAssertEqual(dp.x, 10)
        XCTAssertEqual(dp.y, 20)
        XCTAssertEqual(dp.w, 2)
        XCTAssertEqual(dp.h, 2)
        XCTAssertEqual(dp.pixels.count, 12)
        XCTAssertEqual(dp.pixels[0], 1.0, accuracy: 0.0001)
    }

    // DATA payload — RGBA tile
    func testParseDataPayloadRGBA() throws {
        let pixels: [Float] = [0.1, 0.2, 0.3, 1.0]  // 1x1 RGBA
        let payload = makeDataPayload(x: 0, y: 0, w: 1, h: 1,
                                       numSamples: 4, pixels: pixels)
        let dp = try TLVParser.parseDataPayload(from: payload)
        XCTAssertEqual(dp.pixels.count, 4)
        XCTAssertEqual(dp.pixels[3], 1.0, accuracy: 0.0001)
    }

    // Partial read simulation — header truncated
    func testPartialReadHeaderThrows() {
        let truncated = Data([0x01, 0x00])  // only 2 bytes instead of 5
        XCTAssertThrowsError(try TLVParser.parseHeader(from: truncated))
    }

    // START payload — truncated (no title bytes when titleLen > 0)
    func testPartialStartPayloadThrows() {
        var payload = makeStartPayload(width: 100, height: 100, numSamples: 3, title: "hello")
        // Remove the title bytes to simulate truncation
        payload = payload.dropLast(5)
        XCTAssertThrowsError(try TLVParser.parseStartPayload(from: payload))
    }
}
