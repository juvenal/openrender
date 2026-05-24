import simd
import CRibPreview

// ─── ArcballCamera ───────────────────────────────────────────────────────────
//
// Internal model: stores viewMatrix (= orender's camera-to-world `from`) and
// orbCtrBaked (orbit pivot in baked space = from * worldPoint).
//
// "Baked space": P_baked = viewMatrix * P_world.  +Z is forward; objects in
// front have positive baked.z.  This convention matches the perspective
// projection (w_clip = baked.z) and is consistent for ANY camera matrix,
// including improper rotations (det = −1) that break quaternion decomposition.
//
// All orbit/pan/zoom operations pre-multiply viewMatrix with a small baked-
// space transform, keeping the orbit pivot fixed in both baked and world space.

@MainActor
final class ArcballCamera {

    // ── Projection ────────────────────────────────────────────────────────────
    private let ribProj: simd_float4x4
    private(set) var projMatrix: simd_float4x4

    // ── View matrix (= orender's `from`, camera-to-world applied to world pts)
    private var viewMatrix: simd_float4x4
    private let initViewMatrix: simd_float4x4

    // ── Orbit center in baked space ───────────────────────────────────────────
    private var orbCtrBaked: simd_float3
    private let initOrbCtrBaked: simd_float3

    // Fixed world-space grid origin (= scene center; used by WireframeRenderer)
    let gridOriginWorld: simd_float3

    // ── Saved state for drag/pan operations ──────────────────────────────────
    private var savedViewMatrix:  simd_float4x4 = matrix_identity_float4x4
    private var savedOrbCtrBaked: simd_float3   = .zero
    private var dragFromSphere:   simd_float3   = simd_float3(0, 0, 1)
    private var panFrom:          simd_float2   = .zero

    // ── Window metrics (updated on resize) ───────────────────────────────────
    var windowSize: simd_float2 = simd_float2(800, 600)
    var radius: Float = 500.0

    // MARK: – Init

    init(camera: PreviewCameraC, bounds: PreviewBoundsC) {
        ribProj    = loadMatrix(camera.projMatrix)
        projMatrix = ribProj

        // viewMatrix = from = to^{-1} (world → +Z-forward baked space)
        let from   = loadMatrix(camera.viewMatrix).inverse
        initViewMatrix = from
        viewMatrix     = from

        let bmin = simd_float3(bounds.sceneBoundsMin.0,
                               bounds.sceneBoundsMin.1,
                               bounds.sceneBoundsMin.2)
        let bmax = simd_float3(bounds.sceneBoundsMax.0,
                               bounds.sceneBoundsMax.1,
                               bounds.sceneBoundsMax.2)
        let sceneCenter = (bmin + bmax) * 0.5
        gridOriginWorld = sceneCenter

        // Orbit center = scene center projected into baked space.
        // Falls back to (0,0,diag) when the camera doesn't face the scene.
        let c4 = from * simd_float4(sceneCenter, 1)
        if c4.z > 0.01 {
            initOrbCtrBaked = simd_float3(c4.x, c4.y, c4.z)
        } else {
            let diag = simd_length(bmax - bmin)
            initOrbCtrBaked = simd_float3(0, 0, max(diag, 1.0))
        }
        orbCtrBaked = initOrbCtrBaked
        radius = simd_length(windowSize) * 0.5
    }

    // MARK: – Camera matrices

    var viewProjectionMatrix: simd_float4x4 { projMatrix * viewMatrix }

    // Camera-to-world matrix (for camera export; viewMatrix = 'from').
    var cameraToWorldMatrix: simd_float4x4 { viewMatrix }

    // World-space orbit center for grid placement (fixed = scene center).
    var orbitCenter: simd_float3 { gridOriginWorld }

    // Perspective uses w_clip = baked.z → proj.columns.2.w = 1.0.
    var isOrthographic: Bool { projMatrix.columns.2.w != 1.0 }

    // Vertical FOV in degrees from perspective projection Y scale.
    var fovDegrees: Float {
        let fv = projMatrix.columns.1.y   // 1/tan(vfov/2)
        guard fv > 0 else { return 45.0 }
        return 2.0 * atan(1.0 / fv) * (180.0 / .pi)
    }

    // MARK: – Orbit

    func beginOrbit(at screen: simd_float2) {
        savedViewMatrix = viewMatrix
        dragFromSphere  = toSphere(screen)
    }

    func orbit(to screen: simd_float2) {
        let toSph = toSphere(screen)
        let from  = dragFromSphere
        let axis  = simd_cross(from, toSph)
        let cosA  = simd_dot(from, toSph)
        // Unit quaternion for this arc (rotation in baked space)
        let drag  = simd_normalize(simd_quatf(ix: axis.x, iy: axis.y, iz: axis.z, r: cosA))
        let Q     = simd_float4x4(drag)
        // Pre-multiply: rotate around orbCtrBaked while keeping it fixed
        let Tc    = translate(orbCtrBaked.x,  orbCtrBaked.y,  orbCtrBaked.z)
        let Tn    = translate(-orbCtrBaked.x, -orbCtrBaked.y, -orbCtrBaked.z)
        viewMatrix = matrix_multiply(matrix_multiply(Tc, matrix_multiply(Q, Tn)), savedViewMatrix)
    }

    // MARK: – Pan

    func beginPan(at screen: simd_float2) {
        savedViewMatrix  = viewMatrix
        savedOrbCtrBaked = orbCtrBaked
        panFrom          = screen
    }

    func pan(to screen: simd_float2) {
        let d = screen - panFrom
        // Speed proportional to distance; minimum ensures usable ortho pan
        let speed = max(0.001 * savedOrbCtrBaked.z, 0.005)
        let dtx   = -d.x * speed   // drag right → scene moves right (baked +X)
        let dty   =  d.y * speed   // drag up   → scene moves up   (baked +Y)
        orbCtrBaked = savedOrbCtrBaked + simd_float3(dtx, dty, 0)
        viewMatrix  = matrix_multiply(translate(dtx, dty, 0), savedViewMatrix)
    }

    // MARK: – Zoom (delta > 0 → zoom out, delta < 0 → zoom in)

    func zoom(delta: Float) {
        let oldZ = orbCtrBaked.z
        let newZ = max(oldZ + oldZ * 0.1 * delta, 0.01)
        let dz   = newZ - oldZ
        orbCtrBaked.z = newZ
        viewMatrix = matrix_multiply(translate(0, 0, dz), viewMatrix)
    }

    // MARK: – Reset

    func reset() {
        viewMatrix  = initViewMatrix
        orbCtrBaked = initOrbCtrBaked
        projMatrix  = ribProj
    }

    // MARK: – Resize

    func updateAspect(width: Float, height: Float) {
        windowSize = simd_float2(width, height)
        radius = simd_length(windowSize) * 0.5
        // Keep vertical FOV fixed; adjust horizontal scale for new aspect ratio
        var proj = projMatrix
        proj.columns.0.x = proj.columns.1.y / (width / height)
        projMatrix = proj
    }

    // MARK: – Private helpers

    // Map screen pixel to unit sphere for arcball orbit.
    private func toSphere(_ pt: simd_float2) -> simd_float3 {
        let cx = windowSize.x * 0.5
        let cy = windowSize.y * 0.5
        let dx = (pt.x - cx) / radius
        let dy = -(pt.y - cy) / radius   // viewPoint Y increases downward; flip to sphere Y-up
        let l2 = dx * dx + dy * dy
        if l2 > 1.0 {
            let l = sqrt(l2)
            return simd_float3(dx / l, dy / l, 0)
        }
        return simd_float3(dx, dy, sqrt(1.0 - l2))
    }

}

// ─── Free helpers ─────────────────────────────────────────────────────────────

private func translate(_ x: Float, _ y: Float, _ z: Float) -> simd_float4x4 {
    var m = matrix_identity_float4x4
    m.columns.3 = simd_float4(x, y, z, 1)
    return m
}

func loadMatrix(_ t: (Float,Float,Float,Float,Float,Float,Float,Float,
                       Float,Float,Float,Float,Float,Float,Float,Float))
    -> simd_float4x4
{
    return simd_float4x4(columns: (
        simd_float4(t.0,  t.1,  t.2,  t.3),
        simd_float4(t.4,  t.5,  t.6,  t.7),
        simd_float4(t.8,  t.9,  t.10, t.11),
        simd_float4(t.12, t.13, t.14, t.15)
    ))
}
