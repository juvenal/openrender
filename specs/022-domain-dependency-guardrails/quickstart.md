# Quickstart: validating the domain-dependency guardrails (spec 022)

Runnable checks that prove the feature end to end. Formats and exit codes are in [contracts/](contracts/); entities are in [data-model.md](data-model.md).

## Prerequisites

- A completed build of this worktree: `cmake -B build -S . && cmake --build build -j`. The check reads object files and depfiles, so the build must be complete.
- `nm` on `PATH`: Xcode command-line tools on macOS, binutils on Linux.

## 1. The check passes on the current tree (US1 #1, SC-002, SC-004)

```bash
ctest --test-dir build -L architecture --output-on-failure
```

Expected:
- every `architecture` test passes;
- `Architecture_DomainBoundaries` ends with `RESULT PASS`;
- `ALLOWED` lists every registry ID with its edge count;
- `CYCLE` lines (if any) are all marked `[allowlisted]`.

To see the full report directly:

```bash
build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture
```

## 2. Injected violations fail with actionable messages (US1 #2–#4, SC-001)

These run automatically as fixture scenarios (research R10):

```bash
ctest --test-dir build -L architecture -R 'ArchFixture_' --output-on-failure
```

Each fixture asserts its exit code and message:

| Scenario | Expected |
|---|---|
| forbidden include | `VIOLATION include …  [GraphicsState -> Shading]` and exit 1 |
| forbidden link | `VIOLATION link … defined in: …` and exit 1 |
| new cycle | `CYCLE … ` without `[allowlisted]`, and exit 1 |

Manual spot check on the real tree (revert afterwards, don't stage):

```bash
echo '#include "shader.h"' >> src/ri/state/xform.h      # Graphics State -> Shading
cmake --build build -j && build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture
# expect: VIOLATION include src/ri/state/xform.h -> src/libshader/shading/shader.h ... and exit 1
git checkout -- src/ri/state/xform.h && cmake --build build -j
```

## 3. Registry and allowlist integrity (US2, FR-009/010/010a)

Fixture scenarios cover these cases:

| Case | Expected |
|---|---|
| stale entry | `STALE`, exit 1 |
| unknown ID | `BADID … no Open registry entry`, exit 1 |
| resolved ID | `BADID`, exit 1 |
| entry tagged for the other platform | ignored, exit 0 |
| entry whose file isn't compiled in this build | `NOTBUILT`, exit 0 (never stale) |
| same edge tagged and untagged | configuration error, exit 2 |

Integrity of the real files (manual review):
- every `DB-NNN` in `tests/architecture/allowlist.txt` appears as an Open heading in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`;
- every DB-005+ entry has rule, evidence, size, direction and tracking issue(s);
- `specs/022-domain-dependency-guardrails/registry-reconciliation.md` maps each audit finding V1–V21 to exactly one of: DB-NNN, "see DB-004", or "gone" with a reason (SC-003).

## 4. Renderer-core metric (US3, FR-012, SC-007)

```bash
for i in 1 2; do build/tests/architecture/archcheck --source-dir . --build-dir build \
  --config tests/architecture --format json > /tmp/archcheck-$i.json; done
cmp /tmp/archcheck-1.json /tmp/archcheck-2.json && echo deterministic
```

Expected: `deterministic`. The whole report, metrics included, is byte-identical across runs. The `ArchFixture_metric_increase` scenario shows a `WARNING metric …` line with exit 0.

## 5. Vestigial includes removed (US4, FR-013)

- `git diff master -- src/` shows only removed `#include` lines, each listed in `research.md` R12 (plus any the tool found).
- Clean rebuild with `-Werror`; full `ctest` green; the check no longer reports those edges.
- GCC 13 / Linux: the build is validated through the Linux path. This is required before merge (spec US4 #1); there is no "unverified" escape.

## 6. Runtime budget (SC-005)

```bash
time build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture --quiet
```

Expected: well under 60 s. The `nm` pass alone measured 2.2 s over 248 objects.

## 7. CI coverage (FR-001)

No workflow change is needed. Confirm that `.github/workflows/release.yml` runs plain `ctest` in its Ubuntu and both macOS jobs, so `Architecture_DomainBoundaries` runs there by default.
