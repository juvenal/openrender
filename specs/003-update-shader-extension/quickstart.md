# Quickstart: Shader Extension Update (.sdr to .rslo)

## 1. Compile a Shader (Default)

The compiler now outputs `.rslo` files by default:

```bash
oshader surface.sl
# Output: surface.rslo
```

## 2. Compile for Legacy Systems

Use the `--legacy-sdr` flag to produce the old `.sdr` format:

```bash
oshader --legacy-sdr surface.sl
# Output: surface.sdr
```

## 3. Render a Scene

The renderer handles both extensions automatically:

```bash
orender scene.rib
```

*   If `my_shader.rslo` exists, it will be loaded.
*   If `my_shader.rslo` is missing but `my_shader.sdr` exists, the renderer will use the legacy shader and log an informational message.

## 4. Inspect a Shader

`sdrinfo` also supports the dual-extension lookup:

```bash
sdrinfo surface
# or
sdrinfo surface.sdr
# or
sdrinfo surface.rslo
```
