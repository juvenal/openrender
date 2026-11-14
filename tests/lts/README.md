# LTS verification suite

`ctest --test-dir build -L '^lts$'` runs these checks. They are part of the
ordinary suite as well, and the `lts-verify` CI job on `release/*` requires
them (with `abi-dumper`) to pass.

| Test | What it guards |
| --- | --- |
| `Lts_PluginAbiHeaders` | Public C headers compile together, and each plug-in family's packed ABI version matches the MAJOR/MINOR/PATCH macros beside it. |
| `Lts_SoVersion` | Every shared library's SONAME uses `OPENRENDER_COMPAT_SOVERSION`. |
| `Lts_ExportedSymbols` | `tests/lts/required-symbols.txt` names are still exported (`RiBegin`, `RiEnd`, `RiWorldBegin`, `RiWorldEnd`, the RGBE readers). |
| `Lts_InstallSmoke` | `cmake --install` produces a prefix where `orender -h` / `orender -v` run and the public headers compile. |
| `Lts_AbiDump` | When `abi-dumper` is installed and the libraries contain DWARF, dump `libRIBRender` and `libRIBCommon` and require the stable C symbols in the dump. A file at `abi/baselines/<library-file-name>.dump` is compared with `abi-compliance-checker`. |
| `Lts_PolicyFile` | `LTS.json` describes a two-year window and each recorded line's EOL is the cut date plus two years. |
| `Lts_SwiftParse` | On a machine with `swiftc`, parse the macOS framebuffer and wire-viewer sources and the XCTest files under `tests/framebuffer/`. |

`abi-dumper` and `abi-compliance-checker` are optional for a local `ctest`.
The release-branch job sets `OR_LTS_REQUIRE_ABI_TOOLS=1` and installs them, so
a missing tool or a library built without `-g` fails that job.

No baseline dump is checked in. Capturing one freezes a whole C++ ABI and
will go stale while master is moving. To start comparing on an LTS line:

```bash
abi-dumper build/src/ri/libRIBRender.so.1.0.0 -o tests/lts/abi/baselines/libRIBRender.so.1.0.0.dump -lver 1
abi-dumper build/src/common/libRIBCommon.so.1.0.0 -o tests/lts/abi/baselines/libRIBCommon.so.1.0.0.dump -lver 1
```

Build that tree with debug info (`-g`) or the dump is empty. The filename
must match `get_filename_component(NAME)` of `$<TARGET_FILE:...>`, which is
the real versioned library name CMake produces.

The XCTest files are not an SPM test target. `Package.swift` for the macOS
framebuffer only declares an executable, so `swift test` has nothing to run.
`Lts_SwiftParse` is a syntax check until those files are wired into a test
target. The macOS app itself is still built by CMake (`swift build`).
