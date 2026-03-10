# Data Model: Compiled Shader Extension

This feature updates the data model for how compiled shaders are represented and located in the `openrender` system.

## Entities

### Compiled Shader (Object)

*   **Extension**: `.rslo` (default, primary), `.sdr` (legacy, secondary).
*   **Attributes**: 
    *   **Base Name**: The name of the shader, e.g., `plastic`.
    *   **Full Filename**: `plastic.rslo` or `legacy_shader.sdr`.
*   **Behavior**:
    *   **Default Generation**: Compiler produces `.rslo`.
    *   **Fallback Resolution**: Renderer and tools prioritize `.rslo`.

### Search Path Protocol

*   **Input**: Shader name (e.g., `plastic`, `plastic.sdr`, `plastic.rslo`).
*   **Search Path**: `SHADERPATH` environment variable.
*   **Search Priority**:
    1.  If explicit extension `.sdr` provided: strip `.sdr`.
    2.  Try `[path]/[base].rslo`.
    3.  If not found, try `[path]/[base].sdr`.
    4.  If not found, report error.

## Validation Rules

1.  **Unique Base Names**: A single directory SHOULD NOT contain both `shader.rslo` and `shader.sdr` to avoid ambiguity. If it does, `.rslo` is always chosen.
2.  **Explicit Path Handling**: If the user provides a full filesystem path, the same extension-stripping and dual-lookup applies.
