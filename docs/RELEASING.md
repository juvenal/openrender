# Releasing openRender

The default branch is `master`. Release lines live on `release/X.Y` branches
cut from a master commit. Nothing in these workflows pushes to `master`.

| Workflow | When it runs | What it publishes |
| --- | --- | --- |
| [`.github/workflows/ci.yml`](../.github/workflows/ci.yml) | Every pull request, merge-queue group, and a manual run | Nothing. It is the required status checks. |
| [`.github/workflows/nightly.yml`](../.github/workflows/nightly.yml) | 03:00 UTC, and manual | A dated `nightly-*` prerelease. |
| [`.github/workflows/cut-lts.yml`](../.github/workflows/cut-lts.yml) | Manual | `release/X.Y`, tag `vX.Y.0`, and a pull request that records the end of support. |
| [`.github/workflows/release-lts.yml`](../.github/workflows/release-lts.yml) | A version tag that sits on a `release/*` branch | A normal GitHub release. |
| [`.github/workflows/backport.yml`](../.github/workflows/backport.yml) | A merged pull request with a backport label | Pull requests onto release branches. |
| [`.github/workflows/lts-maintenance.yml`](../.github/workflows/lts-maintenance.yml) | Mondays 06:00 UTC, and manual | Nothing. It dispatches CI, or opens an issue when a line is past end of support. |

The older [`.github/workflows/release.yml`](../.github/workflows/release.yml) still publishes a master-only version tag (self-contained and FHS tarballs, a universal macOS package, Debian and Fedora packages, the Homebrew formula). Its Intel job runs on `macos-15-intel` and its Apple Silicon jobs on `macos-15`. GitHub no longer provides the `macos-13` runner, and `macos-14` cannot be started for this repository. A tag that is also contained in a `release/*` branch is left to `release-lts.yml`, so the two workflows do not both publish it.

## Pull request CI

`CI` runs on `ubuntu-24.04` (GCC 13 and Clang, both linking the distro LLVM) and `macos-15` (Apple Clang, Homebrew LLVM for the JIT). `macos-14` is not a runner this repository can start. Flex and Bison come from apt, or from Homebrew on macOS because the system Bison is too old. ccache is restored per OS, compiler, and build type.

| Check name | What it runs |
| --- | --- |
| `CI / linux-gcc-release` | Release build. Full CTest suite, then the determinism sweep. |
| `CI / linux-clang-release` | Same, with Clang. |
| `CI / macos-clang-release` | Same, with Apple Clang. This build also compiles the Swift framebuffer and wire viewer. |
| `CI / linux-gcc-debug` | Debug build. Full suite except tests labelled `visual` or `determinism-sweep`. |
| `CI / macos-clang-debug` | Same Debug selection. |
| `CI / lts-verify` | Release build with debug info, `ctest -L '^lts$'`, and `abi-dumper`. Runs only for `release/*`. |

"Full suite" here is the project's everyday run (`ctest -LE determinism-sweep`: unit, architecture, visual, plug-in, shading) followed by the opt-in sweep (`ctest -L '^determinism-sweep$'` at `-j2`). Debug builds skip the visual tests and the sweep. Those tests have timeouts chosen for a Release (`-O3`) binary; a Debug render of the same scenes trips the timeouts without saying anything about correctness.

The Swift XCTest files under `tests/framebuffer/` are not an SPM test target (`Package.swift` only declares the executable). `Lts_SwiftParse` syntax-checks them with `swiftc -parse` when `swiftc` is on the machine. The apps themselves are built by CMake on the macOS jobs.

## Nightlies

The schedule is `0 3 * * *` (03:00 UTC) and runs from `master` only, which is how GitHub schedules work. `workflow_dispatch` checks out `master` as well, so dispatching the workflow from another branch does not publish that branch. Set **force** to build even when master has not moved.

Before building, the workflow compares `master` with the newest `nightly-*` tag. If they are the same commit, it stops. A skipped run is a success, not a failure.

Dated tags (`nightly-YYYYMMDD`, or `nightly-YYYYMMDD-<sha>` when that day's tag already points at a different commit) are used instead of a moving `nightly` tag. A moving tag has to be force-pushed, and a build that was current yesterday is then unnamed. Dated tags stay put. The publish job keeps the 14 newest `nightly-*` prereleases and deletes the older ones, including their tags. `make_latest` is off, so a nightly does not become the repository's Latest release.

Each nightly runs the full suite on Ubuntu (GCC) and macOS (Apple Clang), then packs the self-contained install tree with CPack:

- Linux: `.tar.gz` and a `.deb` whose files live under `/opt/openrender`
- macOS: `.tar.gz` for the arm64 runner (`macos-15`)

The packages are attached to the prerelease. No commit is pushed to `master`.

## Cutting an LTS line

An LTS line is supported for two years from the UTC date it is cut. The date is stored in [`LTS.json`](../LTS.json). There are no lines until the first cut.

1. Actions → **Cut LTS** → Run workflow.
2. **version** is `X.Y` (for example `1.0`). **commit** is optional; leave it empty to use the tip of `master`. The commit must be contained in `master`.
3. The workflow pushes `release/X.Y` at that commit, tags `vX.Y.0`, and opens a pull request against `master` that adds the line to `LTS.json` and a Dependabot entry for that branch. Review and merge that pull request. It does not change the release branch.
4. The tag push starts **LTS release**, which builds, tests, and publishes `vX.Y.0`.

End of support is the cut date plus two years (29 February becomes 28 February). `Lts_PolicyFile` rejects a record that does not match.

## Publishing a later LTS release

Fixes land on `release/X.Y` through backport pull requests. When that branch is ready:

```bash
git checkout release/X.Y
git tag -a vX.Y.Z -m "openRender X.Y.Z"
git push origin vX.Y.Z
```

`X.Y.Z` is the numeric version. A suffix such as `-rc.1` is published as a prerelease. The tag has to be contained in a `release/*` branch. Pushing the tag is what starts the workflow; do not push the release commit to `master`.

The Ubuntu and macOS jobs run the full suite (including the `lts` label). The Fedora job builds the RPM and runs the `lts` tests. The visual suite is not repeated on Fedora: its reference images are captured on the Ubuntu and macOS baselines, and the same commit has already been compared there. Artifacts:

- `openrender-X.Y.Z-linux-<arch>.tar.gz` and `.deb` (self-contained tree under `/opt/openrender`)
- `openrender-X.Y.Z-macos-<arch>.tar.gz`
- `openrender-X.Y.Z-linux-<arch>.rpm` from Fedora

`abi-dumper` is required on `release/*` pull requests (`CI / lts-verify`, built with `-g`). The tag build ships the ordinary Release binaries, without debug info, so `Lts_AbiDump` skips there unless a baseline dump has been checked in. See [`tests/lts/README.md`](../tests/lts/README.md).

## Backporting

The backport workflow uses [korthout/backport-action](https://github.com/korthout/backport-action) v5. It opens pull requests. It does not push to the release branch.

On the merged pull request, add one of:

- `backport release/X.Y` for that line
- `backport` for every line in `LTS.json` that is still inside its window

You can add the label before or after the merge. A line past its end-of-support date is not a target of the bare `backport` label.

Backport pull requests created with the default `GITHUB_TOKEN` do not start other workflows. Set a repository secret `BACKPORT_TOKEN` (a fine-grained PAT, or a GitHub App token, with Contents and Pull requests write) so CI runs on them. The workflow uses that secret when it is present.

On a public repository, from 2 December 2026 GitHub does not run `pull_request_target` unless an Actions policy allows it. In the repository settings, allow `pull_request_target` for `.github/workflows/backport.yml`. Until that policy exists, backport labels will not start the workflow.

## Weekly maintenance and end of support

Mondays at 06:00 UTC the maintenance workflow reads `LTS.json` on `master`.

- A line whose end-of-support date is still in the future gets a `workflow_dispatch` of CI on that branch.
- A line past that date gets an issue titled `LTS past EOL: release/X.Y`, labelled `lts-eol`, if one is not already open. CI is not dispatched. The branch is not deleted.
- A `release/*` branch that `LTS.json` does not mention gets an issue titled `Untracked release branch: ...`.

When you accept that a line is finished, delete the branch (an admin has to bypass the ruleset, which blocks deletion) and remove its block from `.github/dependabot.yml`.

## Dependabot

[`.github/dependabot.yml`](../.github/dependabot.yml) updates GitHub Actions weekly and opens the pull requests against `master`. `cut-lts.yml` adds a block for `release/X.Y` in the record pull request, above the `# LTS-DEPENDABOT-ENTRIES` marker. Dependabot reads that file from the default branch, so the release branch does not need its own copy.

## Branch protection

Rulesets cannot be applied from a pull request. The importable document is [`.github/rulesets/protect-master-and-release.json`](../.github/rulesets/protect-master-and-release.json). It holds two rulesets:

- **Protect master** on `refs/heads/master`
- **Protect release branches** on `refs/heads/release/**`

Both require a pull request, one approval, a review from the code owner when a owned path changes, the CI checks named above, and the merge queue (merge commits, one pull request at a time, six hours for a check to finish). Force-push and deletion are blocked. `CI / lts-verify` is required only on `release/*`. It does not run on `master`, and a skipped job must not be a required check.

Repository admins are a bypass actor with mode `pull_request`. A single maintainer cannot approve their own pull request, so without that bypass the ruleset cannot be satisfied by the owner alone. Admins can merge a pull request that has the required checks. They still cannot force-push: the bypass mode is not `always`. Remove that actor from the JSON before importing if a second reviewer is available and you want the approval to be unconditional.

`do_not_enforce_on_create` is on for release branches so `cut-lts.yml` can create `release/X.Y` before any check has run on it. Later commits still need a pull request.

### Apply with the API

From a checkout, authenticated as an admin:

```bash
bash .github/rulesets/apply.sh
```

The script creates a ruleset or updates one with the same name. Pass `owner/repo` if the current directory is not the GitHub checkout.

### Apply in the UI

Settings → Rules → Rulesets → New ruleset → Import. The importer takes one ruleset object, not the wrapper. Copy each element of the `rulesets` array into its own file, import it, and set enforcement to Active. After import, open the ruleset and confirm the bypass actor shows as the Admin role (repository role id 5).

Also confirm:

- Settings → General → Pull Requests → allow merge commits (the queue uses merge commits).
- Settings → Actions → General → Workflow permissions: read and write is not required for CI (each workflow sets its own permissions). **Allow GitHub Actions to create and approve pull requests** must be on, or `cut-lts.yml` cannot open the record pull request and the backport action cannot open its pull requests.
- The Actions policy for `pull_request_target` described above.

## What the hosted runner does not match

The everyday suite still runs on the GCC Release job, including every visual test. A few checks compare bytes with a file captured on the maintainer's machine, or misbehave only at `-O0`, and those names are excluded in `scripts/ci/run-ctest.sh`:

- `TextureTile_RenderByteIdentical`. The runner's libtiff writes the IFD at a different offset (the files differ at byte 5). The visual suite's block average is the image check.
- `Registry_IntegratorImageIdentity`. The camera hiders matched. Two one-thread photon maps from the same process did not.
- On the Clang Release job only: `Motion_Replay_tessellation` (Clang contracts a multiply-add where the GCC replay does not) and `Registry_TestDoubleHider` (one run counted 7 bucket jobs for a 32×32 frame with 16×16 buckets; the check wants 4, and the GCC Release job still runs it).
- `DeterminismSweep_sphere-texture3d-bake3d-reyes`, both the interpreter entry and the `-slo` entry. The interpreter entry segfaulted on Clang at the default thread count, and the `-slo` entry segfaulted on GCC at `-t:4`. The name is a prefix, so both are out of the sweep on every hosted runner. The visual suite does not include this sweep.
- `ShadingParity_JitPL_trace_samples_cone` is part of that visual suite. Its 120-second property elapses while four tests share the runner (it passed in 85 seconds on the macOS runner and timed out at 120 on Ubuntu). The script skips it in the parallel pass, raises the generated timeout to 600 seconds, and runs it alone. The source `TIMEOUT` is unchanged.
- On Debug jobs only: `Blobby_Tolerance` (timeout at `-O0`), `Geometry_DiceThroughDicer` and `Geometry_ExpansionThroughHost` (they crash at `-O0`), and `Link_ShadingSelfContained` (a Debug `libshader_shading` imports `std::string` symbols that the check also finds in `libRIBRender`).
- On macOS Release: `Shaders_BytesUnchanged` (Homebrew LLVM writes different `.slo` bitcode than `reference/macos/shaders.sha256`) and every `ShadingParity_JitPL_envshadow_raytrace_` probe. `bias`, `maxdist`, `blur`, and `samples_cone` were not bit-identical under Apple Clang, and `raytrace_base` is skipped with them so the next probe does not fail the job by itself. The engaged controls (`ShadingParity_JitPLEngaged_`) still run. The GCC Release job runs the whole family.
- On macOS Debug, `scripts/ci/exclude-macos-debug.txt` lists the rest. Apple Clang at `-O0` prints different float text, moves display samples by one ulp, keeps `CRenderer` static references the architecture allowlist does not record, and hits debug asserts (`stats.cpp`, `memory.cpp`, `surface.cpp` tessellation memory, shader string counts) that the GCC Debug binary does not. `^Parity_` skips every parity scene: `shutdownTesselations` aborted `Parity_csg-hero-displaced-raytrace-reyes` on one run and `Parity_depthdefault` on the next. That pattern does not match `ShadingParity_`. Linux GCC Debug still runs those tests.

The configure uses Unix Makefiles. `ShadingScope_Greps` reads `flags.make` and `ShadingContract_StandaloneLinkLine` reads `link.txt`; a Ninja build does not leave those files where the tests look.

Install and packaging use `DESTDIR`. `OPENRENDER_HOMEDIR` is the absolute `CMAKE_INSTALL_PREFIX` from configure time (`/opt/openrender` in these workflows), so `cmake --install --prefix` cannot move `.orenderrc`. The self-contained bundle step builds a destination that already includes `DESTDIR` and then calls `file(INSTALL)`, which prefixes `DESTDIR` again, so on macOS the Homebrew libraries landed outside `prefix/lib` and `orender -h` could not load `libtiff`. `scripts/ci/stage-install.sh` rewrites that one call in the generated `cmake_install.cmake` to `file(COPY)` before installing. The source `CMakeLists.txt` is left as it is.

## What the LTS tests check

`tests/lts/` is a normal CTest label, hooked in with one `add_subdirectory(lts)` line. The checks are API and packaging, not image diffs:

- plug-in headers compile and each family's ABI macros agree
- every shared library's SONAME is `OPENRENDER_COMPAT_SOVERSION`
- `RiBegin`, `RiEnd`, `RiWorldBegin`, `RiWorldEnd`, and the RGBE entry points are still exported
- `cmake --install` yields a tree where `orender -h` and `orender -v` run, and the same program compiles against the installed headers
- `abi-dumper` output contains those C symbols when the tool and debug info are present; a dump dropped in `tests/lts/abi/baselines/` is compared with `abi-compliance-checker`
- `LTS.json` describes a two-year window

No ABI baseline dump is checked in. A full C++ dump goes stale while `master` is moving. Capture one on the day a line is cut if you want the compliance checker to fail the branch later; the filename has to match the real library file name. Details are in `tests/lts/README.md`.
