import simd
import CRibPreview

// ─── ArcballCamera ───────────────────────────────────────────────────────────
//
// Port of CInterface from src/gui/interface.h.
//
// Internal model:
//   worldToCamera = T(0,0,-distance) * R(orientation) * T(orbitCenter)
//
// The orbit center is the world-space point that stays screen-centered during
// orbit (equivalent to -position in CInterface, but sign-flipped for clarity).
// distance ≡ zoom in CInterface.

@MainActor
final class ArcballCamera {

    // RIB camera matrices — stored to support exact reset.
    private let ribProj: simd_float4x4
    private let ribView: simd_float4x4

    // Arcball live state.
    private(set) var projMatrix: simd_float4x4
    private var orbitCenter: simd_float3
    private var orientation: simd_quatf
    private var distance: Float

    // Arcball initial state (matches RIB decomposition) — used by reset().
    private let initOrbitCenter: simd_float3
    private let initOrientation: simd_quatf
    private let initDistance: Float

    // Per-drag saved state.
    private var savedOrientation: simd_quatf = simd_quatf(ix: 0, iy: 0, iz: 0, r: 1)
    private var savedOrbitCenter: simd_float3 = .zero
    private var savedDistance: Float = 1.0
    private var dragFromSphere: simd_float3 = simd_float3(0, 0, 1)
    private var panFrom: simd_float2 = .zero

    // Window metrics (updated on resize).
    var windowSize: simd_float2 = simd_float2(800, 600)
    var radius: Float = 500.0

    // MARK: – Init

    init(camera: PreviewCameraC, bounds: PreviewBoundsC) {
        ribProj = loadMatrix(camera.projMatrix)
        ribView = loadMatrix(camera.viewMatrix)
        projMatrix = ribProj

        // Scene center as orbit center.
        let bmin = simd_float3(bounds.sceneBoundsMin.0,
                               bounds.sceneBoundsMin.1,
                               bounds.sceneBoundsMin.2)
        let bmax = simd_float3(bounds.sceneBoundsMax.0,
                               bounds.sceneBoundsMax.1,
                               bounds.sceneBoundsMax.2)
        let sceneCenter = (bmin + bmax) * 0.5

        // Decompose ribView into orientation + distance given the orbit center.
        // ribView = [R | t]  where t = -R*orbitCenter + (0,0,-distance)
        // → distance = -(R*orbitCenter + t).z
        let (orient, dist) = ArcballCamera.decompose(worldToCamera: ribView,
                                                     orbitCenter: sceneCenter)

        orbitCenter      = sceneCenter
        orientation      = orient
        distance         = dist
        initOrbitCenter  = sceneCenter
        initOrientation  = orient
        initDistance     = dist

        radius = simd_length(windowSize) * 0.5
    }

    // MARK: – Camera matrices

    var viewProjectionMatrix: simd_float4x4 {
        projMatrix * currentViewMatrix
    }

    // Camera-to-world matrix (for camera export)
    var cameraToWorldMatrix: simd_float4x4 {
        currentViewMatrix.inverse
    }

    // Whether the stored projection matrix is orthographic
    var isOrthographic: Bool { projMatrix.columns.3.z != -1.0 }

    // Vertical FOV in degrees extracted from the perspective projection matrix
    var fovDegrees: Float {
        let fv = projMatrix.columns.1.y   // = 1/tan(fov/2)
        guard fv > 0 else { return 45.0 }
        return 2.0 * atan(1.0 / fv) * (180.0 / .pi)
    }

    private var currentViewMatrix: simd_float4x4 {
        let T_dist   = translate(0, 0, -distance)
        let R        = simd_float4x4(orientation)
        let T_center = translate(orbitCenter.x, orbitCenter.y, orbitCenter.z)
        return matrix_multiply(matrix_multiply(T_dist, R), T_center)
    }

    // MARK: – Orbit

    func beginOrbit(at screen: simd_float2) {
        savedOrientation = orientation
        dragFromSphere = toSphere(screen)
    }

    func orbit(to screen: simd_float2) {
        let to   = toSphere(screen)
        let from = dragFromSphere
        let axis = simd_cross(from, to)
        let cosA = simd_dot(from, to)
        let drag = simd_quatf(ix: axis.x, iy: axis.y, iz: axis.z, r: cosA)
        orientation = simd_normalize(simd_mul(drag, savedOrientation))
    }

    // MARK: – Pan

    func beginPan(at screen: simd_float2) {
        savedOrbitCenter = orbitCenter
        panFrom = screen
    }

    func pan(to screen: simd_float2) {
        let d = screen - panFrom
        // Camera-space X and Y axes in world space (columns of cameraToWorld).
        let c2w   = currentViewMatrix.inverse
        let right = simd_float3(c2w.columns.0.x, c2w.columns.0.y, c2w.columns.0.z)
        let up    = simd_float3(c2w.columns.1.x, c2w.columns.1.y, c2w.columns.1.z)
        let speed = 0.001 * distance
        orbitCenter = savedOrbitCenter
                    - right * (d.x * Float(speed))
                    + up    * (d.y * Float(speed))
    }

    // MARK: – Zoom

    // delta > 0 → zoom out (increase distance), delta < 0 → zoom in.
    func zoom(delta: Float) {
        savedDistance = distance
        distance = max(distance * (1.0 + delta * 0.1), 0.01)
    }

    // MARK: – Reset

    func reset() {
        orbitCenter  = initOrbitCenter
        orientation  = initOrientation
        distance     = initDistance
        projMatrix   = ribProj
    }

    // MARK: – Resize

    func updateAspect(width: Float, height: Float) {
        windowSize = simd_float2(width, height)
        radius = simd_length(windowSize) * 0.5

        // Update perspective projection aspect ratio (column-major: proj[0][0]).
        var proj = projMatrix
        if proj.columns.3.z == -1.0 {       // perspective check
            let fv     = proj.columns.1.y   // fv = 1/tan(fov/2)
            let aspect = width / height
            proj.columns.0.x = fv / aspect
            projMatrix = proj
        }
    }

    // MARK: – Private helpers

    // Map screen pixel to unit sphere using arcball projection.
    private func toSphere(_ pt: simd_float2) -> simd_float3 {
        let cx = windowSize.x * 0.5
        let cy = windowSize.y * 0.5
        let dx = (pt.x - cx) / radius
        let dy = -(pt.y - cy) / radius   // screen Y is down, sphere Y is up
        let l2 = dx * dx + dy * dy
        if l2 > 1.0 {
            let l = sqrt(l2)
            return simd_float3(dx / l, dy / l, 0)
        }
        return simd_float3(dx, dy, sqrt(1.0 - l2))
    }

    private static func decompose(worldToCamera m: simd_float4x4,
                                   orbitCenter oc: simd_float3)
        -> (simd_quatf, Float)
    {
        let R = simd_float3x3(columns: (
            simd_float3(m.columns.0.x, m.columns.0.y, m.columns.0.z),
            simd_float3(m.columns.1.x, m.columns.1.y, m.columns.1.z),
            simd_float3(m.columns.2.x, m.columns.2.y, m.columns.2.z)
        ))
        let t = simd_float3(m.columns.3.x, m.columns.3.y, m.columns.3.z)

        // t = -R*oc + (0,0,-distance)  →  distance = -(R*oc + t).z
        let d = -(simd_mul(R, oc) + t).z
        let dist = max(d, 1.0)
        let q = simd_quatf(R)
        return (simd_normalize(q), dist)
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
