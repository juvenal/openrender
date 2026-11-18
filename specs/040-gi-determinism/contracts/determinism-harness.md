# Contract: the determinism harness for GI scenes (FR-016, SC-001)

`tests/visual/test_depth_determinism.cpp` (cross-configuration mode, spec 039) decodes every listed output as a TIFF and compares
it at its own bit depth. GI scenes need three additions; every existing use keeps its behaviour (the options are additive).

| Option | Effect |
|---|---|
| `--files <a.gpm>[,<b.icache>...]` | after each configuration, these files are compared byte for byte with the first configuration's (a size difference or the first differing offset is reported); a missing file is a failure |
| `--float` | puts `Quantize "rgba" 0 0 0 0` after the `--prepend` line, so an 8-bit scene writes float colour and alpha (a scene's own later `Quantize` still wins) |
| `--depth <name.tif>` | adds `Display "+<name.tif>" "file" "z"` after the scene's first `Display` line; the name is then listed in `--outputs` |

Between configurations the harness removes every file named in `--files` from the scratch directory, so no configuration reads a
map or cache that an earlier configuration wrote (the stale-file effect found in research.md R1).

`add_determinism_sweep()` gains `FILES` and `FLOAT`/`DEPTH` keywords that pass these through. The 18 GI scenes of
`determinism-scenes.cmake` move from comments to entries with their map/cache files and `FLOAT DEPTH`; the default configurations
stay `-t:1||-t:2|-t:4|-P:2`. A photon scene whose photon pass cannot run under `-P` (research.md R3) lists its configurations
without `-P:2` and says why on the entry; that is a recorded limit, not a tolerance. The comment on each such entry reads: "photon entries omit -P:2: GitHub #170". `gi-photon-then-indirect` omits `-P:2` too, for the same reason (its photon frame).

Self-test: `DeterminismHarness_*` (spec 039) gains a case that writes two different byte files under `--config-prepends` and must
fail, and one that writes identical files and must pass.
