#include <metal_stdlib>
using namespace metal;

// ─── Pass 1: Scene wireframe geometry ────────────────────────────────────────

vertex float4 sceneVertex(
    uint                        vid       [[vertex_id]],
    const device packed_float3 *positions [[buffer(0)]],
    constant float4x4          &mvp       [[buffer(1)]]
) {
    return mvp * float4(float3(positions[vid]), 1.0);
}

fragment float4 sceneFrag(float4 position [[position]]) {
    return float4(0.85, 0.85, 0.85, 1.0);
}

// ─── Pass 2: Ground-plane grid and XYZ axis gizmo ────────────────────────────

struct GridVert {
    packed_float3 pos;
    packed_float3 col;
};

struct GridOut {
    float4 position [[position]];
    float3 color;
};

vertex GridOut gridVertex(
    uint                     vid  [[vertex_id]],
    const device GridVert   *verts [[buffer(0)]],
    constant float4x4       &mvp  [[buffer(1)]]
) {
    GridOut out;
    out.position = mvp * float4(float3(verts[vid].pos), 1.0);
    out.color    = verts[vid].col;
    return out;
}

fragment float4 gridFrag(GridOut in [[stage_in]]) {
    return float4(in.color, 1.0);
}
