import simd
import CRibPreview

// ─── ArcballCamera ───────────────────────────────────────────────────────────
//
// Internal model:
//   worldToCamera = T(0,0,-distance) * R(orientation) * T(-orbitCenter)
//
// The orbit center is the world-space point that stays screen-centered during
// orbit.  distance > 0 places the camera in front of the orbit center along
// the camera +Z axis (Metal −Z-forward convention).
//
// The initial view is derived from scene bounds rather than from the RIB view
// matrix.  orender's camera convention is +Z-forward (RenderMan standard),
// which is incompatible with the arcball's −Z-forward Metal convention without
// an explicit axis flip; many RIB cameras also contain reflections or
// non-rotation transforms that break quaternion decomposition.  A
// bounds-derived orbit gives a reliable starting view for any scene.

@MainActor
final class ArcballCamera {

    // RIB projection matrix — stored to support exact reset.
    private let ribProj: simd_float4x4

    // Arcball live state.
    private(set) var projMatrix: simd_float4x4
    private(set) var orbitCenter: simd_float3
    private var orientation: simd_quatf
    private var distance: Float

    // Arcball initial state (bounds-derived) — used by reset().
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
        ribProj    = loadMatrix(camera.projMatrix)
        projMatrix = ribProj

        let bmin = simd_float3(bounds.sceneBoundsMin.0,
                               bounds.sceneBoundsMin.1,
                               bounds.sceneBoundsMin.2)
        let bmax = simd_float3(bounds.sceneBoundsMax.0,
                               bounds.sceneBoundsMax.1,
                               bounds.sceneBoundsMax.2)
        let sceneCenter = (bmin + bmax) * 0.5

        // Derive the initial orbit orientation from the RIB camera's view direction.
        //
        // orender is +Z-forward: row 2 of fromWorld (= viewMatrix) gives the
        // world-space direction that the camera looks toward.  Row 1 gives the
        // camera's up vector in world space.
        //
        // The arcball is −Z-forward (Metal convention): cam +Z = opposite of fwd.
        // We build a right-handed camera frame and construct a quaternion from it.
        let ribView = loadMatrix(camera.viewMatrix)
        let fwd     = simd_normalize(simd_float3(ribView.columns.0[2],
                                                  ribView.columns.1[2],
                                                  ribView.columns.2[2]))
        let upApprox = simd_normalize(simd_float3(ribView.columns.0[1],
                                                   ribView.columns.1[1],
                                                   ribView.columns.2[1]))
        let camZ = -fwd   // arcball +Z points away from the scene
        var right = simd_cross(upApprox, camZ)
        if simd_length(right) < 0.001 { right = simd_float3(1, 0, 0) }
        right = simd_normalize(right)
        let actualUp = simd_normalize(simd_cross(camZ, right))

        // R_view is the world-to-camera rotation (rows = right, actualUp, camZ).
        // In simd column-major, rows become the TRANSPOSED columns:
        //   col j = (right[j], actualUp[j], camZ[j], 0)
        let R_view = simd_float4x4(columns: (
            simd_float4(right.x,    actualUp.x, camZ.x,    0),
            simd_float4(right.y,    actualUp.y, camZ.y,    0),
            simd_float4(right.z,    actualUp.z, camZ.z,    0),
            simd_float4(0, 0, 0, 1)
        ))
        let initOrient = simd_quatf(R_view)

        // Match the RIB camera's actual distance to the scene center.
        // ribView is fromWorld (world→camera); its inverse is camera→world,
        // and column 3 is the camera's world-space position.
        let ribCamPos = simd_float3(ribView.inverse.columns.3.x,
                                    ribView.inverse.columns.3.y,
                                    ribView.inverse.columns.3.z)
        let initDist = max(simd_length(sceneCenter - ribCamPos), 0.1)

        orbitCenter     = sceneCenter
        orientation     = initOrient
        distance        = initDist
        initOrbitCenter = sceneCenter
        initOrientation = initOrient
        initDistance    = initDist

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

    // Whether the stored projection matrix is orthographic.
    // In the column-major simd matrix the perspective-divisor sentinel (-1) lives at
    // column 2, row 3 (columns.2.w), which maps to proj[14] in the row-major C layout.
    var isOrthographic: Bool { projMatrix.columns.2.w != -1.0 }

    // Vertical FOV in degrees extracted from the perspective projection matrix
    var fovDegrees: Float {
        let fv = projMatrix.columns.1.y   // = 1/tan(fov/2)
        guard fv > 0 else { return 45.0 }
        return 2.0 * atan(1.0 / fv) * (180.0 / .pi)
    }

    private var currentViewMatrix: simd_float4x4 {
        let T_dist   = translate(0, 0, -distance)
        let R        = simd_float4x4(orientation)
        // T(-orbitCenter): shift the orbit center to the camera origin so that
        // orbiting pivots around the correct world-space point.
        let T_center = translate(-orbitCenter.x, -orbitCenter.y, -orbitCenter.z)
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

        // For both perspective and orthographic, X scale = Y scale / aspect.
        // Perspective: Y scale = fv = 1/tan(vfov/2); Orthographic: Y scale = 1.
        var proj = projMatrix
        proj.columns.0.x = proj.columns.1.y / (width / height)
        projMatrix = proj
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
