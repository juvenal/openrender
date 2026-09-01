# Contract: `orender-wire` CLI Interface

This contract governs `orender-wire`'s command-line surface after this feature. It is the
acceptance target for `test_wire_cli` and for Constitution Principle IV.

## Grammar

```
orender-wire [OPTIONS] <file>

  <file>                 RIB scene, or an openRender data-structure file (photon map,
                         irradiance/gather cache, point cloud, brick map, or a raw
                         debug-geometry dump). Type is auto-detected from content.

  --json                 Headless mode: write a JSON description to stdout and exit.
                         No window is created; no display connection is opened.
  --type=auto|rib|data   Override auto-detection. Default: auto.
  --version              Print "orender-wire <version>" and exit 0.
  -h, --help             Print usage and exit 0.
  --                     End of options.
```

- Unknown option → usage printed to **stderr**, exit 1.
- Exactly one positional operand (`<file>`) is required outside of `--help`/`--version`; zero or
  more than one → usage to stderr, exit 1.
- Options may appear before or after the operand.
- POSIX conventions apply: `--` ends option parsing; long options only (no short-option cluster
  beyond `-h`).

## Exit codes

| Code | Meaning | Reachable under `--json`? |
|---|---|---|
| 0 | Success | Yes |
| 1 | Usage error (bad, missing, or unknown arguments) | Yes |
| 2 | File not found or not readable | Yes |
| 3 | RIB parse failed | Yes |
| 4 | Data file rejected — unrecognized type string, version mismatch, or word-size mismatch | Yes |
| 5 | Graphics initialization failed (no Metal device / no GL 3.3 context) | **No** — `--json` never initializes graphics |

Codes 0–3 are unchanged from `orender-wire`'s current behavior (`main.swift`, `AppDelegate.swift`
today); this feature only adds 4 and 5.

## `--json` output contract

Written to **stdout** only on success; on failure, an error message goes to **stderr** and the
process exits with the matching code above — stdout is never partially written on failure.

Common envelope:

```json
{
  "schemaVersion": 1,
  "tool": "orender-wire",
  "toolVersion": "<string>",
  "file": "<absolute path as given>",
  "documentType": "rib" | "photonmap" | "irradiancecache" | "gathercache"
                | "pointcloud" | "brickmap" | "debugdump" | "unsupported",
  "bounds": { "min": [x, y, z], "max": [x, y, z] },
  "warnings": ["<string>", ...]
}
```

`documentType == "rib"` adds:

```json
"scene": { "lineVertexCount": <int>, "lineSegmentCount": <int> },
"camera": {
  "projectionType": "perspective" | "orthographic",
  "fov": <float>, "frameAspectRatio": <float>,
  "nearPlane": <float>, "farPlane": <float>
}
```

Every other `documentType` (including `"unsupported"`) adds:

```json
"data": {
  "fileVersion": [<int>, <int>, <int>],
  "primitives": { "lines": <int>, "points": <int>, "triangles": <int>, "disks": <int> },
  "decimated": <int>,
  "channels": ["<string>", ...],
  "currentChannel": <int>,
  "detailLevel": <int>,
  "drawMode": "<string>"
}
```

- `documentType == "unsupported"` reports all primitive counts as 0 and a non-empty `warnings`
  entry explaining that no visualization exists for this variant (FR-010); this is a success
  case (exit 0), not an error.
- Fields not applicable to a given document type (`channels` for a debug dump, `detailLevel` for
  anything but a brick map) are present with their "not applicable" sentinel (`[]`, `-1`) rather
  than omitted, so consumers can rely on a stable schema per `schemaVersion`.
- `warnings` also carries the decimation notice when `decimated > 0` (FR-006).

## Compatibility notes

- No compatibility is provided for the removed `oshow` binary's invocation
  (`oshow <file>[,mode]`) — it accepted no real options and is not part of this contract
  (FR-026, spec Assumptions).
- `--help` and `--version` must work with **no** `ORENDERHOME`/`SHADERS`/`DISPLAYS` set,
  identical to the existing RIB-loading path (FR-024).
