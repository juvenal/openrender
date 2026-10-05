# Contract: `archcheck` command line

A test-only tool built under `tests/architecture/`. It is not installed. It follows Principle IV:
- POSIX-style options;
- `--help`;
- human-readable and JSON output;
- report on stdout, diagnostics on stderr;
- Unix exit codes.

## Synopsis

```
archcheck --source-dir <repo> --build-dir <build> --config <dir>
          [--platform macos|linux] [--format text|json]
          [--emit-allowlist] [--registry <file>] [--quiet]
archcheck --help
```

| Option | Meaning | Default |
|---|---|---|
| `--source-dir` | Repository root (paths in the data files are relative to it) | required |
| `--build-dir` | CMake build tree to read objects and depfiles from | required |
| `--config` | Directory holding `domain-map.txt`, `domain-rules.txt`, `allowlist.txt`, `metrics-baseline.txt` | required |
| `--registry` | Registry markdown file | `<source-dir>/DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` |
| `--platform` | Which allowlist tags apply | detected host (`macos` on Darwin, `linux` otherwise) |
| `--format` | Output format | `text` |
| `--emit-allowlist` | Print every current non-allowed edge in allowlist format, with `DB-???` IDs, then exit 0. Bootstrap aid; no checking is done | off |
| `--quiet` | Text mode: print only failures, warnings and the summary line | off |

The ctest registration is `Architecture_DomainBoundaries`, label `architecture`:

```
archcheck --source-dir ${CMAKE_SOURCE_DIR} --build-dir ${CMAKE_BINARY_DIR}
          --config ${CMAKE_SOURCE_DIR}/tests/architecture
```

Fixture scenarios run the same binary with `--config`/`--source-dir`/`--registry` pointed at `tests/architecture/fixtures/<scenario>/`.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Pass. No violations and no invalid or stale allowlist entries. Warnings (metric increases, consumer reach) may be present |
| 1 | Check failed: at least one violation, stale entry, unknown/resolved registry ID, or cycle containing a non-allowlisted edge |
| 2 | Configuration or environment error: unclassified file, malformed data file, cyclic or illegal rule table, `nm` (or `ninja` for Ninja trees) not found, build tree incomplete or not a CMake build |

## Text output (stable shape; tests match on these line prefixes)

```
archcheck: platform=macos objects=248 sources=612 edges=1834 (include=1022 link=812)
VIOLATION include src/ri/state/options.h -> src/libshader/shading/shader.h  [GraphicsState -> Shading] rule: allowed-dependency table (Graphics State depends on none)
VIOLATION link    src/ri/texture/pointCloud.cpp -> _ZN10CRaytracer5traceEP4CRay (CRaytracer::trace(CRay*))  [Texturing -> Hiders] defined in: src/ri/hiders/raytracer.cpp
STALE     DB-012 include src/ri/state/attributes.cpp -> src/ri/hiders/irradiance.h  (edge no longer present; delete the entry)
BADID     DB-099 include src/... -> ...  (no Open registry entry DB-099)
CYCLE     Geometry -> Shading -> Geometry  (via src/ri/geometry/object.h -> .../shading.h; src/libshader/shading/shading.cpp -> .../surface.h)  [allowlisted]
ALLOWED   DB-004 x 57   DB-005 x 212   ...
CONSUMER  src/sloinfo/sloinfo.cpp -> src/libshader/shading/llvmJit.h  [Consumer -> Shading]
METRIC    Geometry refs 88 (baseline 88)  distinct 24 (baseline 24)
WARNING   metric Texturing refs 116 > baseline 115 (+1)
RESULT    FAIL violations=2 stale=1 badid=1 failing-cycles=0
```

- Every `VIOLATION` line carries the file, the target (header or symbol plus its demangled form), both classes, and the rule. That makes FR-007 and SC-001 checkable by regex.
- The final line is always `RESULT PASS …` or `RESULT FAIL …`.

## JSON output (`--format json`)

```json
{
  "platform": "macos",
  "counts": {"objects": 248, "sources": 612, "include_edges": 1022, "link_edges": 812},
  "violations": [{"kind": "include", "from": "...", "to": "...", "display": null,
                  "from_class": "GraphicsState", "to_class": ["Shading"], "rule": "..."}],
  "stale": [{"id": "DB-012", "kind": "include", "from": "...", "to": "..."}],
  "bad_ids": [{"id": "DB-099", "reason": "unknown|resolved", "line": 42}],
  "cycles": [{"path": ["Geometry", "Shading", "Geometry"], "failing": false, "edges": ["..."]}],
  "allowlisted": {"DB-004": 57},
  "consumer_reach": [{"from": "...", "to": "...", "to_class": "Shading"}],
  "metrics": [{"class": "Geometry", "refs": 88, "refs_baseline": 88, "distinct": 24, "distinct_baseline": 24}],
  "result": "PASS"
}
```

Key order and array order are deterministic, sorted by class, then file, then target (SC-007).
