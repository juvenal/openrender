# CLI Contract: Shader Compiler (oshader)

## Command: `oshader`

### New Arguments

| Argument | Type | Default | Description |
|----------|------|---------|-------------|
| `--legacy-sdr` | Flag | N/A | Force the output extension to `.sdr` instead of the default `.rslo`. |

### Behavior Change

*   **Default Execution**: Running `oshader my_shader.sl` produces `my_shader.rslo`.
*   **Legacy Execution**: Running `oshader --legacy-sdr my_shader.sl` produces `my_shader.sdr`.
*   **Explicit Output**: `oshader -o custom.sdr my_shader.sl` will still produce `custom.sdr`.

## Command: `sdrinfo`

### Behavior Change

*   **Shader Lookup**: `sdrinfo my_shader` will now look for `my_shader.rslo` first, then `my_shader.sdr`.
*   **Output**: Informational messages should identify which file format was loaded if fallback occurred.
