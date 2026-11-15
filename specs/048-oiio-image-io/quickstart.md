# Quickstart: validating spec 048

Paths are relative to the worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/048-oiio-image-io`
unless absolute.

Rules for everything below:
- Every build, test, vcpkg run or render goes through `~/.claude/bin/cpujob <label> …` at `-j4` at most (`-j2` on
  the Linux host), with vcpkg's own parallelism capped by `VCPKG_MAX_CONCURRENCY`.
- Check `df -h /Volumes/Projects` first, and stop below 2 GB.
- Section M commands M0, M1 and M3a are approved (see the conditions at the head of section M); M3b and M4 run at implementation.

## Build trees

Until the merge, **no tree of this worktree is configured against the shared `~/.cache/vcpkg-installed/openrender`**.
A configure there, even with an unchanged closure, rewrites its `vcpkg/manifest-info.json` and status files. Once
this branch's `vcpkg.json` carries the feature, it would write this branch's manifest into the tree every other
worktree uses. Both trees use the **same dependency mode**, so the with/without comparison (V2) compares OIIO and
nothing else:

| Tree | Configure (vcpkg mode, `VCPKG_ROOT` set) | vcpkg install dir |
|---|---|---|
| `build/` (OIIO off) | `cmake -S . -B build -DVCPKG_INSTALLED_DIR=$HOME/.cache/vcpkg-installed/openrender-048-base` | private base tree, restored from the binary cache (M0) |
| `build-oiio/` (OIIO on) | `cmake -S . -B build-oiio -DOPENRENDER_ENABLE_OIIO=ON` | derived `~/.cache/vcpkg-installed/openrender-oiio` (M1); configure fails if it would be the shared one |

Both private trees need the maintainer's approval (M0, M1), because they run vcpkg. The no-vcpkg alternative for
early phases is Homebrew mode for **both** trees (`env -u VCPKG_ROOT cmake …`, OIIO off and on). It uses Homebrew
OIIO 3.2.2, not the vendored 3.1.14, so the vcpkg pair must still be validated before review. After the merge, the
OIIO-off tree may use the shared install tree like every other worktree.

## V. Validation scenarios

- **V1, default build byte-identity (US2, FR-014, SC-004).** Build `build/` (OIIO off).
  - `ctest --test-dir build -LE determinism-sweep -j4`: all pass, with no reference or threshold file in the diff
    (`git diff --stat -- tests/ src/libshader/tests/reference` shows only new files).
  - `ctest --test-dir build -L imageio`: only `ImageIO_AbsentBuild` and `ImageIO_SelectWithoutOIIO` run, and both
    pass.
- **V2, with and without OIIO byte-identity (US2-2).** Build both trees and render
  `examples/rib/camera-dof.rib` plus the image_input and texture_tile scenes from each. Compare every pair with
  `cmp`: identical. `otexmake` on `tests/unit/image_input/fixtures/medium_rgb.tif` in each of the four CLI modes
  gives byte-identical outputs across the trees (this also re-runs `ImageInput_TiffBakeByteIdentical`).
- **V3, architecture (US5-2, US5-3).** `ctest --test-dir build-oiio -L architecture`: pass, and
  `git diff -- tests/architecture/allowlist.txt` is empty. The fixtures `imageio-allowed` (exit 0) and
  `imageio-reverse` (exit 1) behave as designed. Constitution, `domain-rules.txt`, `domain-map.txt` and
  `DOMAIN_BOUNDARIES.md` all say 1.5.0 and name ImageIO the same way.
- **V4, decode parity (US1, SC-001).** `ctest --test-dir build-oiio -R 'ImageIO_Decode(Parity|Errors)'`: 0 differing
  samples over every fixture. Errors name the file and reason. JPEG, TGA, BMP and WebP are refused.
- **V5, difference list (FR-012, SC-009).** `ctest --test-dir build-oiio -R 'ImageIO_DecodeDifferences|ImageIO_DifferenceListDoc'`:
  every entry is confirmed, and the doc and data file agree.
- **V6, selection (FR-013).** `ctest --test-dir build-oiio -R ImageIO_Selection`:
  - the Option is parsed, and an unknown value reports and leaves the backend unchanged;
  - precedence is call, then Option, then default;
  - `otexmake -imageio bogus` exits 2;
  - the disk-cache name gets `.oiio` only on the OIIO path.

  In `build/`, `ImageIO_SelectWithoutOIIO` checks the P-Q6 behaviour.
- **V7, baking (US3, SC-002).** `ctest --test-dir build-oiio -R 'ImageIO_Bake(Parity|Render|Exclusions)'`:
  - every directory's tag set and samples are equal for plain, shadow, cubic, spherical (C++ entry) and cylinder,
    at 8-bit, 16-bit 1/2-channel and float;
  - a 16-bit RGB bake with `oiio` is written by the legacy writer, with the message;
  - the renders with either texture pass at the scene's existing threshold, and identity is reported.
- **V8, display (US4, SC-003).** `ctest --test-dir build-oiio -R 'ImageIO_Display(Parity|Load|FormatErrors)'`:
  - 0 differing samples for TIFF 8/16/float, PNG 8/16, EXR half and RGBE `rgb`, each pair decoded by libtiff,
    libpng or OpenEXR in test code (the OIIO reader only for RGBE);
  - the module passes `checkPluginVersion`;
  - `Display "x.foo" "oiio"`, and `type "openexr"` without OpenEXR, fail and write nothing.
- **V9, single copy (US5-1, FR-004, SC-005).**
  `cmake --install build-oiio --prefix /tmp/or048-install && ctest --test-dir build-oiio -R ImageIO_SingleCopy`
  (install **without** `--strip`: a stripped install drops local symbols and the "exactly one definer" check would
  fail falsely).
  The test runs `nm -a` (all symbols, `CMAKE_NM`) on every Mach-O/ELF file under the prefix and checks:
  - with static OIIO, exactly one file (`libRIBImageIO`) defines `OpenImageIO_v3*` symbols;
  - with shared OIIO (Homebrew or the Linux prefix), no project file defines one and only `libRIBImageIO` references
    one;
  - `libRIBImageIO` exports none (`nm -gU`).
- **V10, colour (P-Q10).** `ctest --test-dir build-oiio -R ImageIO_NoColourConversion`: decoded samples are equal with
  `OCIO` unset and with `OCIO` set to a different config, for a cICP PNG and for PNGs the driver writes.

## M. Measurements (M0, M1, M3a approved 2026-11-14 with conditions; they are tasks of Phase 1 in tasks.md)

Conditions the maintainer set (P-Q14): M0 and M1 checksum the shared tree's `vcpkg/status` and
`vcpkg/manifest-info.json` before and after and **stop if either changes**; M1 holds vcpkg's lock, so it runs only
when the coordinator gives a slot and no other worktree is configuring; M3a runs on the Linux host in `~/oiio-048`
through the remote `cpujob` at `-j2` and never touches `openrender`, `cellar`, `shrimp`, `openrender-029` or
`openrender-038` there; everything on macOS goes through `~/.claude/bin/cpujob`, `-j4` at most, with
`VCPKG_MAX_CONCURRENCY=4`.

### M0. macOS: the private base tree (OIIO off, vcpkg mode)

```bash
WT=/Volumes/Projects/Development/CLI/openrender-worktrees/048-oiio-image-io
S="$HOME/.cache/vcpkg-installed/openrender"
shasum "$S/vcpkg/status" "$S/vcpkg/manifest-info.json" > /tmp/or048-shared-before.sha
VCPKG_MAX_CONCURRENCY=4 ~/.claude/bin/cpujob or048-m0-base-cfg \
  cmake -S "$WT" -B "$WT/build" -DVCPKG_INSTALLED_DIR="$HOME/.cache/vcpkg-installed/openrender-048-base" \
  > /tmp/or048-m0.log 2>&1
grep -E 'Restored [0-9]+ package|Building ' /tmp/or048-m0.log    # expect restores only
du -sh "$HOME/.cache/vcpkg-installed/openrender-048-base"
shasum -c /tmp/or048-shared-before.sha                             # shared tree untouched
```

### M1. macOS: OIIO through vcpkg into a private install tree (first-build cost, size, LLVM reuse)

This runs `vcpkg install` in manifest mode against a scratch copy of the manifest with the `oiio` feature added, into
the same private directory the OIIO-on configure will later use. It never names the shared
`~/.cache/vcpkg-installed/openrender`. It writes new entries only to vcpkg's scratch directories
(`~/.vcpkg/buildtrees`, `packages`, `downloads`) and the shared binary cache (`~/.cache/vcpkg/archives`, additive).
It takes `~/.vcpkg/vcpkg-running.lock`, so run it when no other worktree is configuring.

```bash
WT=/Volumes/Projects/Development/CLI/openrender-worktrees/048-oiio-image-io
PRIV="$HOME/.cache/vcpkg-installed/openrender-oiio"
df -h /Volumes/Projects "$HOME" | cat
du -sh "$HOME/.cache/vcpkg/archives" "$HOME/.cache/vcpkg-installed/openrender"
S="$HOME/.cache/vcpkg-installed/openrender"
shasum "$S/vcpkg/status" "$S/vcpkg/manifest-info.json" > /tmp/or048-shared-before.sha
mkdir -p /tmp/or048-vcpkg-probe
python3 -I - "$WT/vcpkg.json" /tmp/or048-vcpkg-probe/vcpkg.json <<'EOF'
import json, sys
m = json.load(open(sys.argv[1]))
m.setdefault("features", {})["oiio"] = {
    "description": "OpenImageIO 3 image backend (spec 048 probe)",
    "dependencies": [{"name": "openimageio", "default-features": False}]}
json.dump(m, open(sys.argv[2], "w"), indent=2)
EOF
VCPKG_MAX_CONCURRENCY=4 ~/.claude/bin/cpujob or048-m1-vcpkg-oiio \
  /usr/bin/time -l "$VCPKG_ROOT/vcpkg" install \
    --x-manifest-root=/tmp/or048-vcpkg-probe \
    --x-install-root="$PRIV" \
    --triplet=arm64-osx-openrender \
    --overlay-triplets="$WT/CMake/vcpkg-triplets" \
    --x-feature=oiio \
    --clean-after-build \
  > /tmp/or048-m1.log 2>&1
grep -E 'Restored [0-9]+ package|will be built|Building |Installing |Total install time|real|maximum resident' /tmp/or048-m1.log
grep -E 'llvm:arm64-osx-openrender' /tmp/or048-m1.log      # expect "restored", never "Building"
du -sh "$PRIV" "$PRIV/arm64-osx-openrender/lib/libOpenImageIO"*.a "$PRIV/arm64-osx-openrender/lib/libOpenColorIO"*.a
du -sh "$HOME/.cache/vcpkg/archives"                        # growth = OIIO's new archives
du -sh "$HOME/.cache/vcpkg-installed/openrender"            # 176M before
shasum -c /tmp/or048-shared-before.sha                       # shared tree untouched (status, manifest-info)
```

Records: wall time, peak RSS, number of ports built and restored, private tree size, OIIO and OCIO archive sizes,
binary-cache growth, and proof that LLVM was restored. The last line proves the shared tree did not move.

### M2. CI cache-key effect (analysis done; live confirmation optional)

Research R11 derives the effect from `release.yml` and vcpkg's per-package ABI hashing: a changed key, restore by
prefix, no LLVM rebuild, about one more 188 MB cache entry. M1's `llvm … restored` line confirms the ABI-hash half
locally. A live confirmation is the next tag build's log ("Restored N package(s)", and no `llvm` build line). It is
optional, observed whenever the maintainer next tags, and no task depends on it.

### M3. Linux footprint (Ubuntu 24.04 validation host)

M3a, the prefix build of OIIO 3.1.14.0 (needs network on the host; no sudo; about 9.3 GB free measured):

```bash
H=juvenal@ubuntu-2404-desktop.v2-labs.io
ssh "${H}" 'mkdir -p ~/oiio-048/src && cd ~/oiio-048/src && \
  curl -fL -o oiio-3.1.14.0.tar.gz https://github.com/AcademySoftwareFoundation/OpenImageIO/archive/refs/tags/v3.1.14.0.tar.gz && \
  tar xzf oiio-3.1.14.0.tar.gz && df -h ~ | tail -1'
ssh "${H}" 'cd ~/oiio-048 && ~/.claude/bin/cpujob or048-m3a-cfg cmake -S src/OpenImageIO-3.1.14.0 -B build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/oiio-048/prefix -DBUILD_SHARED_LIBS=ON \
  -DOpenImageIO_BUILD_MISSING_DEPS=all -DOIIO_BUILD_TOOLS=OFF -DOIIO_BUILD_TESTS=OFF -DBUILD_TESTING=OFF \
  -DUSE_PYTHON=OFF -DUSE_QT=OFF -DUSE_FFMPEG=OFF -DUSE_FREETYPE=OFF -DUSE_GIF=OFF -DUSE_LIBHEIF=OFF \
  -DUSE_LIBRAW=OFF -DUSE_OPENCV=OFF -DUSE_OPENJPEG=OFF -DUSE_OPENVDB=OFF -DUSE_PTEX=OFF -DUSE_WEBP=OFF \
  -DUSE_JXL=OFF -DUSE_TBB=OFF -DUSE_DCMTK=OFF -DUSE_LIBUHDR=OFF -DINSTALL_DOCS=OFF > ~/oiio-048/cfg.log 2>&1; \
  grep -iE "OpenColorIO|fmt|Robin|building" ~/oiio-048/cfg.log | head -20'
ssh "${H}" 'cd ~/oiio-048 && ~/.claude/bin/cpujob or048-m3a-build /usr/bin/time -v cmake --build build -j2 \
  > ~/oiio-048/build.log 2>&1; tail -25 ~/oiio-048/build.log | grep -E "Elapsed|Maximum resident"; \
  cmake --install build > /dev/null && du -sh prefix prefix/lib && ls -la prefix/lib/*.so* | awk "{print \$5, \$9}"'
```

Records: configure output (which dependencies OIIO built itself), build wall time and peak RSS at `-j2`, prefix size,
and the size of each shared library.

M3b, at the implementation's Linux validation: in a per-spec tree on the host (`~/Projects/Development/CLI/openrender-048`,
synced with `rsync -rlpc --files-from` over `git ls-files`), run:

```bash
~/.claude/bin/cpujob or048-linux-cfg cmake -S . -B build-oiio -DOPENRENDER_ENABLE_OIIO=ON \
  -DOpenImageIO_ROOT=$HOME/oiio-048/prefix
~/.claude/bin/cpujob or048-linux-build cmake --build build-oiio -j2
~/.claude/bin/cpujob or048-linux-test ctest --test-dir build-oiio -L 'imageio|image_input|architecture' -j2 --output-on-failure
```

Then repeat the OFF build (`build/`) with `-LE determinism-sweep -j2`. Record the size of `libRIBImageIO.so`.

### M4. Install size (SC-007), at implementation

After V9's install, on macOS and Linux:

```bash
du -sh /tmp/or048-install
ls -la /tmp/or048-install/lib/libRIBImageIO* /tmp/or048-install/lib/libRIBRender* \
  /tmp/or048-install/*/displays/oiio.dsply
```

Compare with research R14's baseline. Expected: `libRIBRender`, `otexmake`, `orender` and the legacy drivers are
unchanged in size within linker noise (they carry no OIIO code), and the cost sits in `libRIBImageIO` alone.
