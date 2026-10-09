# Quickstart: validating spec 023

These steps validate the feature end to end. Conventions:
- `W` is the worktree `../openrender-worktrees/023-common-infrastructure-floor`.
- `S` is the session scratchpad.
- Builds and tests always run under `nice -n 19 … -j4` at most (fanless M2).

## 0. Baselines (run once, before any source change)

```bash
cd "$W"
# A. frozen baseline install (A side for perf, "before" libraries for SC-006).
#    Snapshot /usr/local first: install(CODE) once leaked outside DESTDIR.
ls -laR /usr/local > "$S/023-usrlocal-before.txt" 2>/dev/null
DESTDIR="$S/023-base" nice -n 19 cmake --install build
ls -laR /usr/local > "$S/023-usrlocal-after.txt" 2>/dev/null
diff "$S/023-usrlocal-before.txt" "$S/023-usrlocal-after.txt"    # expect: no output

# B. SC-006 consumers, built and linked ONLY against the baseline install, one per RI library
#    (RIBRender, RIBVector, RIBOut); keep the three binaries unchanged
#    (tools/compat-consumer/build.sh <install-root> <out-dir>)
cp -a "$S/023-base" "$S/023-inplace"     # in-place root; $S/023-base stays the perf A side
specs/023-common-infrastructure-floor/tools/compat-consumer/build.sh "$S/023-inplace" "$S/023-compat"
#    run.sh <install-root> <tag>: runs all three through their own rpath (no DYLD_/LD_ variables)
#    and writes out-<lib>-<tag>.{rib,tif}
"$S/023-compat/run.sh" "$S/023-inplace" base  # expect: exit 0 for each
# B2. determinism probe: run again and compare
"$S/023-compat/run.sh" "$S/023-inplace" base2
for f in "$S"/023-compat/out-*-base.*; do cmp "$f" "${f/-base./-base2.}" || echo "NONDETERMINISTIC: $f"; done

# C. shading-library "before" list (macOS, already captured)
ls "$S/023-libshader-undef-before.txt"
```

**Linux "before" lists.** The user runs these on the Linux host, from master's `build-022`:

```bash
cd /home/juvenal/Projects/Development/CLI/openrender
nm -D --undefined-only build-022/src/libshader/shading/libshader_shading.so.1 | c++filt \
  > /tmp/023-libshader-undef-before-linux.txt
SHADERS="$PWD/build-022/shaders" ORENDERHOME="$PWD/openrender" \
LD_DEBUG=bindings build-022/src/orender/orender examples/rib/camera-dof.rib 2>&1 \
  | grep 'file=.*libshader_shading' | grep -E 'error|warning|info|stats|memory|globalMemory|parseVariable|sample.*Hemisphere|CRefCounter|CFileResource' \
  > /tmp/023-libshader-bindings-before-linux.txt
wc -l /tmp/023-libshader-*-before-linux.txt
```

## 1. Build and the new tests

```bash
cd "$W"
nice -n 19 cmake --build build --config Release -j4
nice -n 19 ctest --test-dir build -L common --output-on-failure -j4
```

All of `Common_*` must pass:

| Test | What a pass shows |
|---|---|
| `VariableDeclarations` | The golden table is unchanged |
| `VocabularyConsistency` | `ri.h`, `riVocabulary.h` and `riTokens.cpp` list the same tokens |
| `StateOwnership` | Each state symbol has one definer, `libRIBCommon` |
| `DiagnosticsNoContext_*` | Each severity goes to the right stream; only fatal ends the process, with status 255 |
| `DiagnosticsCrossLibrary`, `DiagnosticsCrossLibraryStatic` | One sink is shared across libraries, in the shared and static-package configurations |
| `ArenaSemantics` | Nested begin/end and save/restore behave as before |
| `ShadingInfraBinding` | The 17 symbols resolve from `libRIBCommon` |
| `VariableLookupHook` | The Graphics State lookup hook works and is installed by the renderer core |

## 2. Architecture check (US1, FR-010/011)

```bash
nice -n 19 ctest --test-dir build -L architecture --output-on-failure -j4
grep -c '^DB-016' tests/architecture/allowlist.txt          # expect: 0
grep -c '^DB-001' tests/architecture/allowlist.txt          # expect: ≤ 18; every removal was reported stale by the check
git diff master -- tests/architecture/allowlist.txt | grep '^+[^+]'   # expect: no added lines
git diff master -- tests/architecture/metrics-baseline.txt          # only decreases
```

## 3. Shading-library binding (US2, SC-002)

```bash
nm -m build/src/libshader/shading/libshader_shading.1.dylib | c++filt \
  | grep -E 'error\(|warning\(|info\(|stats|globalArena|memory(Init|Tini|NewPage)|parseVariable|sample(Cosine)?Hemisphere|CRefCounter|CFileResource'
# expect: each line "(from libRIBCommon)" or a local definition, never "(dynamically looked up)"
nm -m build/src/libshader/shading/libshader_shading.1.dylib | grep 'dynamically looked up' | c++filt \
  | diff - <(grep 'dynamically looked up' "$S/023-libshader-undef-before.txt" | c++filt)
# expect: only removals (the 17 infrastructure symbols); what remains is DB-001's scope
```

On Linux, the user runs:
- `readelf -d .../libshader_shading.so.1 | grep libRIBCommon`
- `nm -D --defined-only .../libRIBRender.so.1 | c++filt | grep -E 'error\(|parseVariable|stats$'`,
  which must print nothing
- the `LD_DEBUG=bindings` command from §0, which now shows the bindings going to
  `libRIBCommon.so.1`.

## 4. Diagnostics without a context (US3)

```bash
build/tests/unit/common/test_common_diagnostics error   ; echo "status=$?"  # stderr, status 0
build/tests/unit/common/test_common_diagnostics warning ; echo "status=$?"  # stdout, status 0
build/tests/unit/common/test_common_diagnostics info    ; echo "status=$?"  # stdout at INFO, status 0
build/tests/unit/common/test_common_diagnostics fatal   ; echo "status=$?"  # stderr, status 255
```

## 5. Compatibility (US4, SC-006)

```bash
# "Installed in their place": install the new tree OVER the in-place root from §0 and run the
# UNCHANGED §0 binaries (built before any source change) with no DYLD_/LD_ variables
DESTDIR="$S/023-inplace" nice -n 19 cmake --install build      # with the /usr/local snapshot/diff of §0
"$S/023-compat/run.sh" "$S/023-inplace" new                    # expect: exit 0 for each, libRIB* resolved inside the root
for l in RIBRender RIBVector RIBOut; do
  # cmp for outputs the §0 probe found deterministic; block metric otherwise (tasks T004a)
  for f in "$S/023-compat"/out-$l-base.*; do cmp "$f" "${f/-base./-new.}"; done
done
# Fresh install for the remaining checks
DESTDIR="$S/023-new" nice -n 19 cmake --install build
# Installed RI libraries find libRIBCommon when loaded by absolute path (no loader variables)
for l in RIBRender RIBVector RIBOut; do otool -l "$S"/023-new/*/*/lib/lib$l.1.dylib | grep -A2 LC_RPATH; done
# Display plug-ins from the INSTALLED tree find libRIBCommon (TIFF Display through the file driver)
"$S/023-compat/render-installed.sh" "$S/023-new"   # expect: exit 0, a TIFF written, no dlopen error
for l in RIBRender RIBVector RIBOut; do
  otool -l "build/src/ri/lib$l.1.dylib" | grep -A2 LC_REEXPORT_DYLIB | grep -q libRIBCommon \
    && echo "$l re-exports" || echo "$l MISSING re-export"
done
# Source compatibility: ri.h is byte-identical
git diff --quiet master -- src/ri/parse/ri.h && echo "ri.h unchanged"
# Package consumer (shared + Static components) rebuilds and renders unchanged
specs/023-common-infrastructure-floor/tools/compat-consumer/build-package.sh "$S/023-new" "$S/023-pkg"
# orender-wire bundle carries the new library
ls build/orender-wire.app/Contents/Frameworks/ | grep RIBCommon
```

## 5a. Reduced-feature builds (spec Edge Cases, "Build options")

```bash
nice -n 19 cmake -B build-nojit -S . -DOPENRENDER_ENABLE_JIT=OFF
nice -n 19 cmake -B build-noexr -S . -DCMAKE_DISABLE_FIND_PACKAGE_OpenEXR=ON
for d in build-nojit build-noexr; do
  nice -n 19 cmake --build "$d" --config Release -j4 && \
  nice -n 19 ctest --test-dir "$d" -L "common|architecture" --output-on-failure -j4
done
rm -rf build-nojit build-noexr
```

## 6. Full suite and renders (FR-013, SC-004)

```bash
nice -n 19 ctest --test-dir build --output-on-failure -j4
```

Expect the same results as master. The known intermittent tests are listed in the memory
`reference_ctest_flaky_tests.md`; re-run them in isolation before treating a red result as real.

## 7. Performance (FR-016, SC-007)

```bash
# serial, interleaved A/B, 7 rounds per RIB, machine otherwise idle
nice -n 19 specs/023-common-infrastructure-floor/tools/perf_ab.sh \
  "$S/023-base" build 7 | tee "$S/023-perf.txt"
# expect: every scene's median(after)/median(before) <= 1.02
```

## 8. Linux validation (the user runs it)

Use a fresh `build-023` directory on the Linux host, with GCC 13 and `-Werror`. Prepare the rsync,
checksum and command blocks as in spec 022. Then run:
1. `ctest -L architecture`, then `ctest -L common`;
2. the full suite, expecting the same 9 pre-existing non-architecture failures as master;
3. §3's Linux checks, and §5's in-place run (no loader variables; `ldd` instead of `DYLD_PRINT_LIBRARIES`).
