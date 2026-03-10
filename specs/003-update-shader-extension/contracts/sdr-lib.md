# Library Contract: sdr.h (Shader Loading Library)

## Function: `sdrGet`

**Signature**: `TSdrShader *sdrGet(const char *in, const char *searchpath)`

### Behavior Change: Dual-Extension Protocol

*   **Input**: `in` (shader name), `searchpath` (list of directories separated by `:`).
*   **Search Sequence**:
    1.  If `in` ends in `.sdr`, strip the extension for initial lookup.
    2.  Try `[path]/[base].rslo`.
    3.  If `[path]/[base].rslo` fails (e.g., `fopen` returns `NULL`), try `[path]/[base].sdr`.
    4.  If fallback to `.sdr` succeeds, log an informational message: `[INFO] sdr: Falling back to .sdr shader "[base].sdr"`.
    5.  If both fail, continue with the standard "not found" error.
*   **Search Priority**: Always attempt to find a `.rslo` version across ALL directories in `searchpath` before attempting to find a `.sdr` version in ANY directory.

### Backward Compatibility

*   **Existing Calls**: All existing code calling `sdrGet` (e.g., in `orender`, `sdrinfo`) will automatically benefit from this protocol.
*   **Existing Shaders**: Shaders currently in the filesystem with `.sdr` extension will continue to be loaded transparently.
