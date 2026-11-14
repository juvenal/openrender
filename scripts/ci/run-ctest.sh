#!/usr/bin/env bash
# Run the CTest selection used by CI.
#   release  everyday suite, then the opt-in determinism sweep
#   debug    everyday suite without visual tests (their timeouts assume -O3)
#   lts      the LTS label only
set -euo pipefail

mode="${1:?ctest mode}"
build="${2:?build directory}"

cores="$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
if [ "${cores}" -gt 4 ]; then
    cores=4
fi

# Names the hosted runner does not match, on every compiler:
#   TextureTile_RenderByteIdentical — cmp against TIFFs captured with the
#     maintainer's libtiff. On the runner the files differ at the IFD offset
#     (byte 5); the visual suite, which allows a block average, still runs.
#   Registry_IntegratorImageIdentity — reyes/zbuffer/raytrace matched. The
#     photon map from two back-to-back one-thread renders did not.
names="${CTEST_EXCLUDE_NAMES:-TextureTile_RenderByteIdentical|Registry_IntegratorImageIdentity}"
if [ -n "${CTEST_EXCLUDE_NAMES_EXTRA:-}" ]; then
    names="${names}|${CTEST_EXCLUDE_NAMES_EXTRA}"
fi
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ "$(uname -s)" = Darwin ]; then
    # reference/macos/shaders.sha256 was captured with the maintainer's LLVM.
    # Homebrew LLVM on the runner emits different .slo bitcode.
    names="${names}|Shaders_BytesUnchanged$"
    if [ "${mode}" = release ]; then
        # rslo and slo of this one probe are not bit-identical under Apple Clang.
        names="${names}|ShadingParity_JitPL_envshadow_raytrace_bias$"
    fi
    if [ "${mode}" = debug ]; then
        while IFS= read -r line || [ -n "${line}" ]; do
            case "${line}" in
                ''|'#'*) continue ;;
            esac
            names="${names}|${line}"
        done < "${here}/exclude-macos-debug.txt"
    fi
fi
label_exclude=""
case "${mode}" in
    debug)
        # -O0 also trips a timeout and two crashes the Release binary does not,
        # and Link_ShadingSelfContained sees libstdc++ string symbols in a
        # Debug libshader_shading that the check treats as renderer imports.
        names="${names}|Blobby_Tolerance|Geometry_DiceThroughDicer|Geometry_ExpansionThroughHost|Link_ShadingSelfContained"
        label_exclude='visual|determinism-sweep'
        ;;
    release)
        # Its 120s property elapses while four tests share the runner. The
        # pass below runs it alone, so the GCC Release visual suite still
        # includes it.
        names="${names}|ShadingParity_JitPL_trace_samples_cone"
        label_exclude='determinism-sweep'
        ;;
esac

common=(--test-dir "${build}" --output-on-failure --no-tests=error -E "${names}")

# The property lives in the generated CTest file. Raising it there leaves the
# source timeout, which is right for a quiet machine, unchanged.
raise_timeout() {
    python3 - "$1" "$2" "$3" <<'PY'
import re
import sys
from pathlib import Path

root, name, seconds = sys.argv[1:]
needle = f"[=[{name}]=]"
found = False
for path in Path(root).rglob("CTestTestfile.cmake"):
    lines = path.read_text().splitlines(keepends=True)
    changed = False
    for index, line in enumerate(lines):
        if needle not in line or "TIMEOUT" not in line:
            continue
        new, count = re.subn(r'TIMEOUT "[0-9]+"', f'TIMEOUT "{seconds}"', line, count=1)
        if count != 1:
            sys.exit(f"run-ctest.sh: {path} has no TIMEOUT for {name}")
        lines[index] = new
        changed = True
        found = True
    if changed:
        path.write_text("".join(lines))
if not found:
    sys.exit(f"run-ctest.sh: {name} has no TIMEOUT property under {root}")
PY
}

case "${mode}" in
    release)
        ctest "${common[@]}" -j "${cores}" -LE "${label_exclude}"
        raise_timeout "${build}" ShadingParity_JitPL_trace_samples_cone 600
        ctest --test-dir "${build}" --output-on-failure --no-tests=error \
            -R '^ShadingParity_JitPL_trace_samples_cone$' -j1
        ctest "${common[@]}" -j 2 -L '^determinism-sweep$'
        ;;
    debug)
        ctest "${common[@]}" -j "${cores}" -LE "${label_exclude}"
        ;;
    lts)
        ctest "${common[@]}" -j "${cores}" -L '^lts$'
        ;;
    *)
        echo "run-ctest.sh: unknown mode '${mode}'" >&2
        exit 2
        ;;
esac
