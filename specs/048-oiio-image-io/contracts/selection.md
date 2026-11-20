# Contract: selecting the OIIO paths (FR-013)

The legacy path is the default everywhere. A selection is always visible: an Option, a parameter, a flag or a driver
name. No environment variable selects a backend, and OIIO never stands in for a format the legacy decoders lack.

## RIB: scene option

```ribi
Option "imageio" "backend" ["oiio"]      # or ["legacy"], the default
```

| Token | Type (pre-declared) | Default | Effect |
|---|---|---|---|
| `backend` | `uniform string` | `"legacy"` | Backend for every image the renderer decodes or bakes from this point: unbaked texture sources (spec 020 synthesized source), the spec 020 disk-cache bake, and every `Make*` call without its own `"imageio"` parameter |

- Render-global, like every Option.
- The option does not affect baked textures, which the texture system reads through its existing path (FR-018), or
  display drivers.
- An unknown value: `error(CODE_BADTOKEN, "Unknown image backend \"%s\" (expected \"legacy\" or \"oiio\")")`; the
  previous value stays.
- `"oiio"` in a build without OIIO: `error(CODE_BADTOKEN, "OpenImageIO support is not built into this binary;
  Option \"imageio\" \"backend\" stays \"legacy\"")` once; the value stays `legacy` (P-Q6).
- An unknown token under `"imageio"`: the existing `RiOptionV` fallthrough error.

Layers (CLAUDE.md "Attributes system", applied to an option):
1. Tokens `RI_IMAGEIO` (`"imageio"`) and `RI_BACKEND` (`"backend"`), added to all three lists: `ri.h`,
   `common/riVocabulary.h` and `common/riTokens.cpp` (gotcha 22; `Common_VocabularyConsistency`).
2. Parse in `RiOptionV` (`rendererContext.cpp`, beside `RI_TEXTURECACHE`).
3. Store in `COptions::imageBackend` (Graphics State vocabulary value).
4. Pre-declare `"imageio" "backend"` (and the per-call `"imageio"`) as `uniform string` in Graphics State's `CDeclarations`
   (`src/ri/state/declarations.cpp`, beside `RI_TEXTURECACHEENABLE`, which ribVector compiles).

## RIB: per call

```ribi
MakeTexture "in.png" "out.tex" "periodic" "periodic" "gaussian" 2 2 "string imageio" ["oiio"]
```

The same parameter is accepted on `MakeShadow`, `MakeLatLongEnvironment` and `MakeCubeFaceEnvironment` (P-Q15).
`"imageio"` is pre-declared as `uniform string`.

- Precedence: the call's parameter, then the Option, then `"legacy"`.
- In a build without OIIO, `"oiio"` fails the call with the "not built" error and writes no file.

## otexmake

```text
otexmake [-imageio legacy|oiio] [existing flags…] input output
```

- Passes `"imageio"` to `makeTexture`/`makeSideEnvironment`/`makeCubicEnvironment`/`makeCylindericalEnvironment`.
- A missing or unknown value prints the error and the usage, and exits 2.
- `oiio` without OIIO prints "OpenImageIO support is not built into this binary" and exits 1 with no output file.
- Documented in `--help` and `man/otexmake.1`.

## Display

```ribi
Display "beauty.exr" "oiio" "rgba"
Display "beauty"     "oiio" "rgb" "string type" ["tiff"]
```

- Driver name `"oiio"` loads `oiio.dsply` through `CDisplayHost` (display ABI 2.0.0, `checkPluginVersion`).
- Format: `"string type"` (`tiff`, `png`, `openexr`, `rgbe`) wins, then the extension (`.tif`, `.tiff`, `.png`,
  `.exr`, `.hdr`, `.pic`). Anything else is an error naming the file, and nothing is written (P-Q7).
- In a build without OIIO the module does not exist, and the host's existing "cannot load display driver" error
  applies.
- The legacy names `file`, `tiff`, `png`, `openexr`, `rgbe` and `framebuffer` are unchanged and are not aliases
  (FR-020).

## Build

`-DOPENRENDER_ENABLE_OIIO=ON|OFF`, default `OFF`. See [dependencies-and-build.md](dependencies-and-build.md).

## Tests (C++)

`createImageInput(path, EImageBackend::Legacy|OIIO)` forces a decoder. A `Make*`/`make*` call with `"imageio"`
forces a writer. Writer-parity tests decode each pair with one fixed reader that is neither writer: libtiff, libpng
or OpenEXR in test code (libtiff for bakes), and the OIIO `Reader` only for RLE RGBE display output.
