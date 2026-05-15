# Quickstart: Imager Shaders in openRender

---

## Writing an Imager Shader

Create a `.sl` file with `imager` as the shader type:

```sl
/* shaders/background.sl — fill transparent areas with a background color */
imager background(
    color bgcolor  = 1;    /* default: white  */
    float background = 1;  /* intensity scale */
)
{
    Ci += (1 - alpha) * (bgcolor * background);
    Oi =  1;
}
```

Compile it with `oshader`:

```sh
oshader shaders/background.sl
# Produces: shaders/background.rslo
```

---

## Using the Imager in a RIB File

Add the `Imager` statement **before** `WorldBegin`:

```rib
Display "output.tif" "file" "rgb"
Format 640 480 1
Projection "perspective" "float fov" [45]

Imager "background"
    "color bgcolor"    [0.2 0.4 0.8]
    "float background" [1.0]

WorldBegin
    # ... geometry ...
WorldEnd
```

Render it:

```sh
SHADERS="$(pwd)/shaders" orender scene.rib
```

---

## Logging

Control imager diagnostic output via `OPENRENDER_LOG_LEVEL`:

```sh
OPENRENDER_LOG_LEVEL=debug SHADERS="$(pwd)/shaders" orender scene.rib
```

| Level | What you see |
|-------|-------------|
| `debug` | Per-bucket execution, variable bind values |
| `info` | Shader name when loaded (default) |
| `warn` | Misplaced `Imager` calls (after WorldBegin) |
| `error` | Shader not found, execution failure |
| `none` | Silent |

---

## Common Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `Cannot locate shader 'background'` | Shader file not on shader search path | Set `SHADERS` env var or `RiOption "searchpath" "shader"` |
| `Imager 'X' specified after WorldBegin; ignored` | `Imager` statement placed after `WorldBegin` | Move `Imager` statement before `WorldBegin` |
| No imager effect visible | Shader compiles to `imager` type but no `Imager` RIB statement present | Add `Imager "background"` to the RIB |

---

## Validation

Run the regression suite to confirm no-imager path is unaffected:

```sh
cmake --build build --config Release
ctest --test-dir build -L imager -V
```
