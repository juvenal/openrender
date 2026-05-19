import MetalKit
import AppKit
import simd
import CRibPreview

// ─── Grid/axis vertex layout ─────────────────────────────────────────────────

private struct GridVertexData {
    var px, py, pz: Float   // packed_float3 position (12 bytes)
    var cr, cg, cb: Float   // packed_float3 color    (12 bytes)
}

// ─── WireframeRenderer ───────────────────────────────────────────────────────
//
// MTKView subclass.  Uses enableSetNeedsDisplay=true so draw(in:) runs on the
// main thread.  Input events call setNeedsDisplay after updating the arcball.

@MainActor
final class WireframeRenderer: MTKView, MTKViewDelegate {

    private var arcball: ArcballCamera!

    private var commandQueue: MTLCommandQueue!
    private var scenePipeline: MTLRenderPipelineState!
    private var gridAxisPipeline: MTLRenderPipelineState!
    private var depthState: MTLDepthStencilState!
    private var sceneBuffer: MTLBuffer?
    private var sceneVertexCount: Int = 0
    private var gridAxisBuffer: MTLBuffer!
    private var gridAxisVertexCount: Int = 0

    // Track drag type for mouseMoved / mouseDragged disambiguation.
    private enum DragKind { case orbit, pan }
    private var activeDrag: DragKind? = nil

    // MARK: – Init

    init(metalDevice: MTLDevice, scene: UnsafePointer<PreviewSceneC>) {
        super.init(frame: .zero, device: metalDevice)
        delegate                    = self
        colorPixelFormat            = .bgra8Unorm
        depthStencilPixelFormat     = .depth32Float
        clearColor                  = MTLClearColor(red: 0.1, green: 0.1, blue: 0.1, alpha: 1.0)
        isPaused                    = true
        enableSetNeedsDisplay       = true

        buildPipelines(device: metalDevice)
        buildDepthState(device: metalDevice)
        buildSceneBuffer(device: metalDevice, scene: scene)
        buildGridAxisBuffer(device: metalDevice)

        arcball = ArcballCamera(camera: scene.pointee.camera,
                                bounds: scene.pointee.bounds)
    }

    required init(coder: NSCoder) { fatalError("not used") }

    override var acceptsFirstResponder: Bool { true }

    // MARK: – Metal setup

    private func buildPipelines(device: MTLDevice) {
        let src = """
        #include <metal_stdlib>
        using namespace metal;

        vertex float4 sceneVertex(
            uint                        vid       [[vertex_id]],
            const device packed_float3 *positions [[buffer(0)]],
            constant float4x4          &mvp       [[buffer(1)]]
        ) { return mvp * float4(float3(positions[vid]), 1.0); }

        fragment float4 sceneFrag(float4 pos [[position]]) {
            return float4(0.85, 0.85, 0.85, 1.0);
        }

        struct GridVert { packed_float3 pos; packed_float3 col; };
        struct GridOut  { float4 position [[position]]; float3 color; };

        vertex GridOut gridVertex(
            uint                   vid   [[vertex_id]],
            const device GridVert *verts [[buffer(0)]],
            constant float4x4     &mvp   [[buffer(1)]]
        ) {
            GridOut o;
            o.position = mvp * float4(float3(verts[vid].pos), 1.0);
            o.color    = verts[vid].col;
            return o;
        }

        fragment float4 gridFrag(GridOut in [[stage_in]]) {
            return float4(in.color, 1.0);
        }
        """

        guard let library = try? device.makeLibrary(source: src, options: nil) else {
            fatalError("orender-wire: Metal shader compilation failed")
        }

        // Scene pipeline (packed_float3, stride 12)
        let sd = MTLRenderPipelineDescriptor()
        sd.vertexFunction   = library.makeFunction(name: "sceneVertex")
        sd.fragmentFunction = library.makeFunction(name: "sceneFrag")
        sd.colorAttachments[0].pixelFormat = .bgra8Unorm
        sd.depthAttachmentPixelFormat      = .depth32Float
        let svd = MTLVertexDescriptor()
        svd.attributes[0].format = .float3; svd.attributes[0].offset = 0
        svd.attributes[0].bufferIndex = 0;  svd.layouts[0].stride    = 12
        sd.vertexDescriptor = svd
        scenePipeline = try! device.makeRenderPipelineState(descriptor: sd)

        // Grid+axis pipeline ({packed_float3 pos, packed_float3 col}, stride 24)
        let gd = MTLRenderPipelineDescriptor()
        gd.vertexFunction   = library.makeFunction(name: "gridVertex")
        gd.fragmentFunction = library.makeFunction(name: "gridFrag")
        gd.colorAttachments[0].pixelFormat = .bgra8Unorm
        gd.depthAttachmentPixelFormat      = .depth32Float
        let gvd = MTLVertexDescriptor()
        gvd.attributes[0].format = .float3; gvd.attributes[0].offset = 0
        gvd.attributes[0].bufferIndex = 0
        gvd.attributes[1].format = .float3; gvd.attributes[1].offset = 12
        gvd.attributes[1].bufferIndex = 0;  gvd.layouts[0].stride    = 24
        gd.vertexDescriptor = gvd
        gridAxisPipeline = try! device.makeRenderPipelineState(descriptor: gd)

        commandQueue = device.makeCommandQueue()!
    }

    private func buildDepthState(device: MTLDevice) {
        let desc = MTLDepthStencilDescriptor()
        desc.depthCompareFunction = .less
        desc.isDepthWriteEnabled  = true
        depthState = device.makeDepthStencilState(descriptor: desc)!
    }

    private func buildSceneBuffer(device: MTLDevice, scene: UnsafePointer<PreviewSceneC>) {
        let n = Int(scene.pointee.vertexCount)
        guard n > 0, let src = scene.pointee.vertices else { return }
        sceneBuffer = device.makeBuffer(bytes: src,
                                        length: n * 3 * MemoryLayout<Float>.size,
                                        options: .storageModeShared)
        sceneVertexCount = n
    }

    private func buildGridAxisBuffer(device: MTLDevice) {
        var v = [GridVertexData]()
        v.reserveCapacity(90)

        // Ground-plane grid (Y=0), ±10 units, 1-unit spacing
        let gc: (Float, Float, Float) = (0.28, 0.28, 0.28)
        for i in -10...10 {
            let f = Float(i)
            v.append(GridVertexData(px: -10, py: 0, pz: f, cr: gc.0, cg: gc.1, cb: gc.2))
            v.append(GridVertexData(px:  10, py: 0, pz: f, cr: gc.0, cg: gc.1, cb: gc.2))
            v.append(GridVertexData(px: f, py: 0, pz: -10, cr: gc.0, cg: gc.1, cb: gc.2))
            v.append(GridVertexData(px: f, py: 0, pz:  10, cr: gc.0, cg: gc.1, cb: gc.2))
        }
        // XYZ axis gizmo (2-unit arms)
        v.append(GridVertexData(px: 0, py: 0, pz: 0, cr: 1.0, cg: 0.2, cb: 0.2))
        v.append(GridVertexData(px: 2, py: 0, pz: 0, cr: 1.0, cg: 0.2, cb: 0.2))
        v.append(GridVertexData(px: 0, py: 0, pz: 0, cr: 0.2, cg: 1.0, cb: 0.2))
        v.append(GridVertexData(px: 0, py: 2, pz: 0, cr: 0.2, cg: 1.0, cb: 0.2))
        v.append(GridVertexData(px: 0, py: 0, pz: 0, cr: 0.2, cg: 0.2, cb: 1.0))
        v.append(GridVertexData(px: 0, py: 0, pz: 2, cr: 0.2, cg: 0.2, cb: 1.0))

        gridAxisBuffer = device.makeBuffer(bytes: v,
                                           length: v.count * MemoryLayout<GridVertexData>.stride,
                                           options: .storageModeShared)!
        gridAxisVertexCount = v.count
    }

    // MARK: – MTKViewDelegate (main thread — enableSetNeedsDisplay=true)

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
        arcball.updateAspect(width: Float(size.width), height: Float(size.height))
        setNeedsDisplay(NSRect(origin: .zero, size: size))
    }

    func draw(in view: MTKView) {
        guard let drawable  = view.currentDrawable,
              let rpDesc    = view.currentRenderPassDescriptor,
              let cmdBuf    = commandQueue.makeCommandBuffer(),
              let encoder   = cmdBuf.makeRenderCommandEncoder(descriptor: rpDesc)
        else { return }

        encoder.setDepthStencilState(depthState)
        var mvp = arcball.viewProjectionMatrix

        if let buf = sceneBuffer, sceneVertexCount > 0 {
            encoder.setRenderPipelineState(scenePipeline)
            encoder.setVertexBuffer(buf, offset: 0, index: 0)
            encoder.setVertexBytes(&mvp, length: MemoryLayout<simd_float4x4>.size, index: 1)
            encoder.drawPrimitives(type: .line, vertexStart: 0, vertexCount: sceneVertexCount)
        }

        encoder.setRenderPipelineState(gridAxisPipeline)
        encoder.setVertexBuffer(gridAxisBuffer, offset: 0, index: 0)
        encoder.setVertexBytes(&mvp, length: MemoryLayout<simd_float4x4>.size, index: 1)
        encoder.drawPrimitives(type: .line, vertexStart: 0, vertexCount: gridAxisVertexCount)

        encoder.endEncoding()
        cmdBuf.present(drawable)
        cmdBuf.commit()
    }

    // MARK: – NSResponder — mouse events (T038)

    override func mouseDown(with event: NSEvent) {
        let pt = viewPoint(event)
        if event.modifierFlags.contains(.option) {
            // Option + left → pan
            activeDrag = .pan
            arcball.beginPan(at: pt)
        } else {
            activeDrag = .orbit
            arcball.beginOrbit(at: pt)
        }
    }

    override func mouseDragged(with event: NSEvent) {
        let pt = viewPoint(event)
        switch activeDrag {
        case .orbit: arcball.orbit(to: pt)
        case .pan:   arcball.pan(to: pt)
        case nil: break
        }
        setNeedsDisplay(bounds)
    }

    override func mouseUp(with event: NSEvent) {
        activeDrag = nil
    }

    override func otherMouseDown(with event: NSEvent) {   // middle button
        activeDrag = .pan
        arcball.beginPan(at: viewPoint(event))
    }

    override func otherMouseDragged(with event: NSEvent) {
        arcball.pan(to: viewPoint(event))
        setNeedsDisplay(bounds)
    }

    override func otherMouseUp(with event: NSEvent) {
        activeDrag = nil
    }

    // Scroll wheel → zoom; two-finger precise scroll → pan (T039)
    override func scrollWheel(with event: NSEvent) {
        if event.hasPreciseScrollingDeltas && event.phase == .changed {
            // Two-finger trackpad pan
            let pt = viewPoint(event)
            if activeDrag == nil {
                activeDrag = .pan
                arcball.beginPan(at: pt)
            }
            let target = simd_float2(
                pt.x + Float(event.scrollingDeltaX),
                pt.y - Float(event.scrollingDeltaY)
            )
            arcball.pan(to: target)
        } else {
            // Scroll wheel or trackpad pinch-like scroll → zoom
            let delta = Float(event.scrollingDeltaY)
            arcball.zoom(delta: delta > 0 ? -1 : 1)
        }
        setNeedsDisplay(bounds)
    }

    // Trackpad pinch → zoom (T039)
    override func magnify(with event: NSEvent) {
        arcball.zoom(delta: Float(-event.magnification) * 10)
        setNeedsDisplay(bounds)
    }

    // R / Home → reset camera (T038)
    override func keyDown(with event: NSEvent) {
        switch event.keyCode {
        case 15: // R
            arcball.reset()
            setNeedsDisplay(bounds)
        case 115: // Home
            arcball.reset()
            setNeedsDisplay(bounds)
        case 1: // S → save camera (T052)
            saveCameraDialog()
        default:
            super.keyDown(with: event)
        }
    }

    // MARK: – Camera export (T052)

    private func saveCameraDialog() {
        let panel = NSSavePanel()
        panel.title                  = "Export Camera"
        panel.nameFieldStringValue   = "camera.rib"
        panel.allowedContentTypes    = [.init(filenameExtension: "rib")!]
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url, let self else { return }
            self.exportCamera(to: url.path)
        }
    }

    private func exportCamera(to path: String) {
        let c2w  = arcball.cameraToWorldMatrix
        var m16  = [Float](repeating: 0, count: 16)
        // Row-major: row r = column r of the column-major simd matrix
        for r in 0..<4 {
            m16[r*4+0] = c2w[0][r]
            m16[r*4+1] = c2w[1][r]
            m16[r*4+2] = c2w[2][r]
            m16[r*4+3] = c2w[3][r]
        }

        let projType = arcball.isOrthographic ? 1 : 0
        let fovDeg   = arcball.fovDegrees
        let exists   = FileManager.default.fileExists(atPath: path)

        let ok = path.withCString { cpath in
            m16.withUnsafeBufferPointer { buf in
                if exists {
                    ribcam_replace(buf.baseAddress, Int32(projType), fovDeg, cpath)
                } else {
                    ribcam_write(buf.baseAddress, Int32(projType), fovDeg, cpath)
                }
            }
        }
        if ok == 0 {
            fputs("orender-wire: failed to save camera to '\(path)'\n", stderr)
        }
    }

    // MARK: – Helpers

    private func viewPoint(_ event: NSEvent) -> simd_float2 {
        let p = convert(event.locationInWindow, from: nil)
        return simd_float2(Float(p.x), Float(frame.height - p.y))  // flip Y to screen-down
    }
}
