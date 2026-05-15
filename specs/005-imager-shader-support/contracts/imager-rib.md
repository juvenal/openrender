# Contract: Imager RIB Interface

**Scope**: The `Imager` statement in RenderMan Interface Bytestream (RIB) files.

---

## Syntax

```rib
Imager "shadername" [parameter-list]
```

- Must appear before `WorldBegin`
- Multiple calls before `WorldBegin`: last call wins
- Called after `WorldBegin`: warning emitted, call ignored

## Parameters

| Field | Type | Description |
|-------|------|-------------|
| `shadername` | string | Name of a compiled imager shader (`.rslo` or `.sdr`) on the shader search path |
| `parameter-list` | key-value pairs | Overrides for shader parameters declared in the shader source |

## Behavior

| Condition | Result |
|-----------|--------|
| Valid shader found | Shader loaded, stored as active imager for the frame |
| Shader not found | Error logged, no imager applied (render continues) |
| Called after WorldBegin | Warning logged, call ignored, prior imager unchanged |
| No `Imager` statement | No-op (zero overhead per pixel) |

## Examples

```rib
# Minimal: background fill with defaults
Imager "background"

# Full: override color and intensity
Imager "background"
    "color bgcolor" [0.2 0.4 0.8]
    "float background" [1.0]

# Replace a previously set imager
Imager "vignette"
    "float radius" [0.8]
Imager "background"     # Only "background" takes effect
```

## Standard Shader Search Path

The renderer resolves `shadername` using `SHADERS` environment variable or `RiOption "searchpath" "shader" [...]`, trying `.rslo` extension first then `.sdr`.
