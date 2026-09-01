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
    private var pointsPipeline: MTLRenderPipelineState!
    private var gridAxisPipeline: MTLRenderPipelineState!
    private var depthState: MTLDepthStencilState!
    private var sceneBuffer: MTLBuffer?
    private var sceneColorBuffer: MTLBuffer?
    private var sceneVertexCount: Int = 0
    private var gridAxisBuffer: MTLBuffer!
    private var gridAxisVertexCount: Int = 0

    // ─── Data-document buffers (spec 016) ───────────────────────────────────
    // Populated only when opening a data document (photon map, cache, point cloud, brick map,
    // debug dump) instead of a RIB scene; a RIB document leaves all three nil, and drawing skips
    // them. Discs arrive already CPU-expanded into triVerts/triCols (see diskExpand.h), so no
    // separate disc pipeline is needed -- they draw through the same triangle pipeline as any
    // other filled geometry (e.g. brick-map boxes).
    //
    // `dataDoc` is retained (not freed after upload) because ribdata_key()'s re-emit cycle
    // (User Story 2) needs the CDataView underneath it to stay alive for the document's whole
    // session -- unlike ribpreview_free's fully-materialized-then-torn-down RIB scene.
    // nonisolated(unsafe): deinit is nonisolated and needs to read this to release the handle;
    // safe because this instance holds the only reference and deinit runs exactly once.
    private nonisolated(unsafe) var dataDoc: OpaquePointer?
    private var dataLineBuffer, dataLineColorBuffer: MTLBuffer?
    private var dataLineVertexCount: Int = 0
    private var dataPointBuffer, dataPointColorBuffer: MTLBuffer?
    private var dataPointVertexCount: Int = 0
    private var dataTriBuffer, dataTriColorBuffer: MTLBuffer?
    private var dataTriVertexCount: Int = 0

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

    // Opens a data document (photon map, irradiance/gather cache, point cloud, brick map, or
    // debug-geometry dump) already produced by ribdata_open(). Takes ownership of `doc` -- it is
    // released in deinit, not here, since ribdata_key() (User Story 2) needs it to outlive this
    // initializer.
    init(metalDevice: MTLDevice, dataDoc doc: OpaquePointer) {
        super.init(frame: .zero, device: metalDevice)
        delegate                    = self
        colorPixelFormat            = .bgra8Unorm
        depthStencilPixelFormat     = .depth32Float
        clearColor                  = MTLClearColor(red: 0.1, green: 0.1, blue: 0.1, alpha: 1.0)
        isPaused                    = true
        enableSetNeedsDisplay       = true

        dataDoc = doc
        buildPipelines(device: metalDevice)
        buildDepthState(device: metalDevice)
        buildGridAxisBuffer(device: metalDevice)

        guard let snapshot = ribdata_snapshot(doc) else {
            fatalError("orender-wire: ribdata_snapshot returned NULL for a just-opened document")
        }
        buildDataBuffers(device: metalDevice, snapshot: snapshot)
        arcball = ArcballCamera(camera: snapshot.pointee.camera, bounds: snapshot.pointee.bounds)
    }

    required init(coder: NSCoder) { fatalError("not used") }

    deinit {
        if let doc = dataDoc {
            ribdata_close(doc)
        }
    }

    override var acceptsFirstResponder: Bool { true }

    // MARK: – Metal setup

    private func buildPipelines(device: MTLDevice) {
        let src = """
        #include <metal_stdlib>
        using namespace metal;

        struct SceneOut { float4 position [[position]]; float3 color; };

        vertex SceneOut sceneVertex(
            uint                        vid       [[vertex_id]],
            const device packed_float3 *positions [[buffer(0)]],
            constant float4x4          &mvp       [[buffer(1)]],
            const device packed_float3 *colors    [[buffer(2)]]
        ) {
            SceneOut o;
            o.position = mvp * float4(float3(positions[vid]), 1.0);
            o.color    = float3(colors[vid]);
            return o;
        }

        fragment float4 sceneFrag(SceneOut in [[stage_in]]) {
            return float4(in.color, 1.0);
        }

        // Points need an explicit point size -- unlike lines/triangles, MTLPrimitiveType.point
        // renders 1-pixel dots unless the vertex shader writes [[point_size]] itself. The
        // sceneVertex/scenePipeline above is reused as-is for triangles (discs arrive already
        // CPU-expanded into the triangle buffer -- see diskExpand.h) by simply changing the
        // MTLPrimitiveType passed to drawPrimitives; only points need a distinct pipeline.
        struct PointOut { float4 position [[position]]; float3 color; float pointSize [[point_size]]; };

        vertex PointOut pointVertex(
            uint                        vid       [[vertex_id]],
            const device packed_float3 *positions [[buffer(0)]],
            constant float4x4          &mvp       [[buffer(1)]],
            const device packed_float3 *colors    [[buffer(2)]]
        ) {
            PointOut o;
            o.position  = mvp * float4(float3(positions[vid]), 1.0);
            o.color     = float3(colors[vid]);
            o.pointSize = 4.0;
            return o;
        }

        fragment float4 pointFrag(PointOut in [[stage_in]]) {
            return float4(in.color, 1.0);
        }

        struct GridVert { packed_float3 pos; packed_float3 col; };
        struct GridOut  { float4 position [[position]]; float3 color; };

        vertex GridOut gridVertex(
            uint                   vid        [[vertex_id]],
            const device GridVert *verts      [[buffer(0)]],
            constant float4x4     &mvp        [[buffer(1)]],
            constant float3       &gridOrigin [[buffer(2)]]
        ) {
            GridOut o;
            o.position = mvp * float4(float3(verts[vid].pos) + gridOrigin, 1.0);
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

        // Points pipeline (same packed_float3 layout as the scene pipeline, stride 12)
        let pd = MTLRenderPipelineDescriptor()
        pd.vertexFunction   = library.makeFunction(name: "pointVertex")
        pd.fragmentFunction = library.makeFunction(name: "pointFrag")
        pd.colorAttachments[0].pixelFormat = .bgra8Unorm
        pd.depthAttachmentPixelFormat      = .depth32Float
        pd.vertexDescriptor = svd
        pointsPipeline = try! device.makeRenderPipelineState(descriptor: pd)

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
        guard n > 0 else { return }
        let byteLen = n * 3 * MemoryLayout<Float>.size
        if let src = scene.pointee.vertices {
            sceneBuffer = device.makeBuffer(bytes: src, length: byteLen,
                                            options: .storageModeShared)
        }
        if let col = scene.pointee.colors {
            sceneColorBuffer = device.makeBuffer(bytes: col, length: byteLen,
                                                 options: .storageModeShared)
        }
        sceneVertexCount = n
    }

    // Uploads a PrimArrayC's verts/cols into a pair of Metal buffers, or leaves them nil if
    // count == 0 -- calling device.makeBuffer with a null/empty pointer is undefined, not a
    // no-op, so every caller must check count first.
    private func uploadPrimArray(device: MTLDevice, array: PrimArrayC) -> (MTLBuffer?, MTLBuffer?, Int) {
        let n = Int(array.count)
        guard n > 0, let verts = array.verts, let cols = array.cols else { return (nil, nil, 0) }
        let byteLen = n * 3 * MemoryLayout<Float>.size
        let vBuf = device.makeBuffer(bytes: verts, length: byteLen, options: .storageModeShared)
        let cBuf = device.makeBuffer(bytes: cols, length: byteLen, options: .storageModeShared)
        return (vBuf, cBuf, n)
    }

    private func buildDataBuffers(device: MTLDevice, snapshot: UnsafePointer<DataSceneC>) {
        let s = snapshot.pointee
        (dataLineBuffer, dataLineColorBuffer, dataLineVertexCount) = uploadPrimArray(device: device, array: s.lines)
        (dataPointBuffer, dataPointColorBuffer, dataPointVertexCount) = uploadPrimArray(device: device, array: s.points)
        (dataTriBuffer, dataTriColorBuffer, dataTriVertexCount) = uploadPrimArray(device: device, array: s.triangles)
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
        setNeedsDisplay(bounds)   // use view-coordinate bounds, not drawable-pixel size
    }

    func draw(in view: MTKView) {
        guard let drawable  = view.currentDrawable,
              let rpDesc    = view.currentRenderPassDescriptor,
              let cmdBuf    = commandQueue.makeCommandBuffer(),
              let encoder   = cmdBuf.makeRenderCommandEncoder(descriptor: rpDesc)
        else {
            // Drawable not ready yet (happens on the very first frame before the
            // CAMetalLayer is fully set up).  Schedule a retry on the next cycle.
            DispatchQueue.main.async { self.setNeedsDisplay(self.bounds) }
            return
        }

        encoder.setDepthStencilState(depthState)
        // Data-document geometry (brick-map boxes, point-cloud/photon discs expanded to
        // triangles, etc.) has no reliable winding order to cull against -- discs in particular
        // are single-sided fans with an arbitrary basis (see diskExpand.h). Disabling culling
        // for the whole pass costs nothing on lines/points, which Metal never culls anyway.
        encoder.setCullMode(.none)
        var mvp = arcball.viewProjectionMatrix

        if let buf = sceneBuffer, sceneVertexCount > 0 {
            encoder.setRenderPipelineState(scenePipeline)
            encoder.setVertexBuffer(buf, offset: 0, index: 0)
            encoder.setVertexBytes(&mvp, length: MemoryLayout<simd_float4x4>.size, index: 1)
            if let colBuf = sceneColorBuffer {
                encoder.setVertexBuffer(colBuf, offset: 0, index: 2)
            }
            encoder.drawPrimitives(type: .line, vertexStart: 0, vertexCount: sceneVertexCount)
        }

        if let buf = dataLineBuffer, dataLineVertexCount > 0 {
            encoder.setRenderPipelineState(scenePipeline)
            encoder.setVertexBuffer(buf, offset: 0, index: 0)
            encoder.setVertexBytes(&mvp, length: MemoryLayout<simd_float4x4>.size, index: 1)
            if let colBuf = dataLineColorBuffer {
                encoder.setVertexBuffer(colBuf, offset: 0, index: 2)
            }
            encoder.drawPrimitives(type: .line, vertexStart: 0, vertexCount: dataLineVertexCount)
        }

        if let buf = dataTriBuffer, dataTriVertexCount > 0 {
            encoder.setRenderPipelineState(scenePipeline)
            encoder.setVertexBuffer(buf, offset: 0, index: 0)
            encoder.setVertexBytes(&mvp, length: MemoryLayout<simd_float4x4>.size, index: 1)
            if let colBuf = dataTriColorBuffer {
                encoder.setVertexBuffer(colBuf, offset: 0, index: 2)
            }
            encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: dataTriVertexCount)
        }

        if let buf = dataPointBuffer, dataPointVertexCount > 0 {
            encoder.setRenderPipelineState(pointsPipeline)
            encoder.setVertexBuffer(buf, offset: 0, index: 0)
            encoder.setVertexBytes(&mvp, length: MemoryLayout<simd_float4x4>.size, index: 1)
            if let colBuf = dataPointColorBuffer {
                encoder.setVertexBuffer(colBuf, offset: 0, index: 2)
            }
            encoder.drawPrimitives(type: .point, vertexStart: 0, vertexCount: dataPointVertexCount)
        }

        var gridOrigin = arcball.orbitCenter
        encoder.setRenderPipelineState(gridAxisPipeline)
        encoder.setVertexBuffer(gridAxisBuffer, offset: 0, index: 0)
        encoder.setVertexBytes(&mvp,        length: MemoryLayout<simd_float4x4>.size, index: 1)
        encoder.setVertexBytes(&gridOrigin, length: MemoryLayout<SIMD3<Float>>.size,  index: 2)
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
