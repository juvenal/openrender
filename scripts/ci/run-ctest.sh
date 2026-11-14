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

common=(--test-dir "${build}" --output-on-failure --no-tests=error)

case "${mode}" in
    release)
        ctest "${common[@]}" -j "${cores}" -LE 'determinism-sweep'
        ctest "${common[@]}" -j 2 -L '^determinism-sweep$'
        ;;
    debug)
        ctest "${common[@]}" -j "${cores}" -LE 'visual|determinism-sweep'
        ;;
    lts)
        ctest "${common[@]}" -j "${cores}" -L '^lts$'
        ;;
    *)
        echo "run-ctest.sh: unknown mode '${mode}'" >&2
        exit 2
        ;;
esac
