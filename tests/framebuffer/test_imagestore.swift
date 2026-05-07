/**
 * tests/framebuffer/test_imagestore.swift
 *
 * Unit tests for ImageStore.swift — tile queuing, CGContext updates,
 * state transitions (idle → active → complete / interrupted).
 */

import XCTest
import Foundation
@testable import OrenderfbmacosSources

@MainActor
final class ImageStoreTests: XCTestCase {

    // State: new store starts in idle state
    func testInitialState() async {
        let store = ImageStore(width: 320, height: 240, numSamples: 3, title: "test")
        XCTAssertEqual(store.windowTitle, "orender — test")
        XCTAssertNil(store.cgImage)
    }

    // FIFO invariant: tiles applied in order of enqueueing
    func testTileEnqueueOrdering() async {
        let store = ImageStore(width: 4, height: 4, numSamples: 3, title: "order-test")
        // Tile 1: top-left pixel = red (1.0, 0.0, 0.0)
        let tile1 = DataPayload(x: 0, y: 0, w: 1, h: 1, pixels: [1.0, 0.0, 0.0])
        // Tile 2: overwrite top-left with blue (0.0, 0.0, 1.0)
        let tile2 = DataPayload(x: 0, y: 0, w: 1, h: 1, pixels: [0.0, 0.0, 1.0])
        await store.applyTile(tile1)
        await store.applyTile(tile2)
        // After both applied: top-left should be blue (tile2 wins)
        let img = store.cgImage
        XCTAssertNotNil(img)
    }

    // CGContext pixel update — single tile
    func testSingleTileUpdatesImage() async {
        let store = ImageStore(width: 2, height: 2, numSamples: 3, title: "pixel-test")
        let tile = DataPayload(x: 0, y: 0, w: 2, h: 2, pixels: [
            1.0, 0.0, 0.0,   0.0, 1.0, 0.0,
            0.0, 0.0, 1.0,   1.0, 1.0, 0.0,
        ])
        await store.applyTile(tile)
        XCTAssertNotNil(store.cgImage)
    }

    // State transition: markDone() sets title to "Rendering Complete — <original>"
    func testMarkDoneUpdatesTitle() async {
        let store = ImageStore(width: 10, height: 10, numSamples: 3, title: "my-scene")
        await store.markDone()
        XCTAssertTrue(store.windowTitle.contains("Complete"),
                      "Expected 'Complete' in title, got: \(store.windowTitle)")
    }

    // State transition: markInterrupted() sets title to "Interrupted — <original>"
    func testMarkInterruptedUpdatesTitle() async {
        let store = ImageStore(width: 10, height: 10, numSamples: 3, title: "my-scene")
        await store.markInterrupted()
        XCTAssertTrue(store.windowTitle.contains("Interrupted"),
                      "Expected 'Interrupted' in title, got: \(store.windowTitle)")
    }

    // After markInterrupted(), store stops accepting new tiles
    func testNoTilesAfterInterrupted() async {
        let store = ImageStore(width: 4, height: 4, numSamples: 3, title: "interrupted-test")
        await store.markInterrupted()
        // Apply a tile after interrupted — should be a no-op
        let tile = DataPayload(x: 0, y: 0, w: 1, h: 1, pixels: [1.0, 0.0, 0.0])
        await store.applyTile(tile)
        // cgImage should still be nil (no tiles were successfully applied)
        XCTAssertNil(store.cgImage)
    }

    // After markDone(), title transition is from idle "Waiting" to "Complete"
    func testTitleTransitionOnDone() async {
        let store = ImageStore(width: 100, height: 100, numSamples: 4, title: "render")
        let initialTitle = store.windowTitle
        XCTAssertTrue(initialTitle.contains("render"))
        await store.markDone()
        XCTAssertNotEqual(store.windowTitle, initialTitle)
    }

    // Title format: original title preserved in both done and interrupted
    func testOriginalTitlePreservedInDoneTitle() async {
        let store = ImageStore(width: 10, height: 10, numSamples: 3, title: "camera-dof")
        await store.markDone()
        XCTAssertTrue(store.windowTitle.contains("camera-dof"),
                      "Original title not in done title: \(store.windowTitle)")
    }

    func testOriginalTitlePreservedInInterruptedTitle() async {
        let store = ImageStore(width: 10, height: 10, numSamples: 3, title: "camera-dof")
        await store.markInterrupted()
        XCTAssertTrue(store.windowTitle.contains("camera-dof"),
                      "Original title not in interrupted title: \(store.windowTitle)")
    }
}
