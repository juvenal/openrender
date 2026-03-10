# Research: Shader Extension Update (.sdr to .rslo)

## Decision: Implementation Path

We will transition the compiled shader extension from `.sdr` to `.rslo` following these steps:

1.  **Compiler Update (`oshader`)**:
    *   Default output extension changed to `.rslo`.
    *   Introduce `--legacy-sdr` CLI flag.
2.  **Shader Loading Library (`sdr`)**:
    *   Modify `sdrGet` in `src/sdr/sdr.y` to implement the dual-search protocol.
    *   Sequence: Try `.rslo` → if fail, try `.sdr` → if fail, standard error.
3.  **Renderer Update (`orender`)**:
    *   Update file type detection in `src/ri/rendererJobs.cpp` to include `.rslo`.
4.  **Tooling Consistency**:
    *   Ensure `sdrinfo` and other tools using the `sdr` library correctly handle the new extension.

## Rationale

*   **LLVM/OSL Compatibility**: `.rslo` is the standard extension for Open Shading Language objects, facilitating future integration.
*   **Backward Compatibility**: Many existing assets use `.sdr`. A silent or informative fallback ensures zero disruption for existing workflows.
*   **User Control**: The `--legacy-sdr` flag allows developers to force the old format if their specific build pipelines require it.

## Alternatives Considered

### 1. Silent Fallback only
*   **Pros**: Simplest to implement.
*   **Cons**: Users won't know they are using deprecated file formats, delaying the transition.
*   **Verdict**: Rejected in favor of informative logging.

### 2. Environment Variable for extension
*   **Pros**: Global control.
*   **Cons**: Harder to manage per-project or per-shader.
*   **Verdict**: Rejected in favor of explicit CLI flag and automatic detection.

## Technical Details

### `oshader` (Compiler)
*   `src/oshader/oshader.cpp`: Main entry point where `outName` is processed.
*   `src/oshader/sl.y`: Bison parser where `CScriptContext::compile` handles the default filename generation.

### `sdr` (Loading Library)
*   `src/sdr/sdr.y`: `sdrGet` function. This is the "choke point" for shader loading.
*   Current logic:
    ```cpp
    sprintf(dest,in);
    if (strstr(dest,".sdr") == NULL) {
        strcat(dest,".sdr");
    }
    sdrin = fopen(tmp,"r");
    ```
*   Proposed logic:
    1.  If `in` has no extension, try `tmp` with `.rslo`.
    2.  If `fopen` fails or `in` has `.sdr` extension (after stripping), try `tmp` with `.sdr`.
    3.  Log informative message on `.sdr` fallback.

### `orender` (Renderer)
*   `src/ri/rendererJobs.cpp`: `processServerRequest` identifies file types by extension to choose the correct `TSearchpath`.
    ```cpp
    if (strstr(fileName, ".sdr") != NULL || strstr(fileName, ".rslo") != NULL)
        search = shaderPath;
    ```
