#!/usr/bin/env bash
# Install a finished build to a temporary prefix, smoke-test orender, and
# pack that tree with CPack. --version is X.Y.Z (CPack's version fields).
# --label is the file-name stem and may carry a prerelease suffix.
set -euo pipefail

build=""
label=""
version=""
output=""
formats="tgz"

while [ $# -gt 0 ]; do
    case "$1" in
        --build) build="$2"; shift 2 ;;
        --label) label="$2"; shift 2 ;;
        --version) version="$2"; shift 2 ;;
        --output) output="$2"; shift 2 ;;
        --formats) formats="$2"; shift 2 ;;
        *)
            echo "usage: package-tree.sh --build DIR --version X.Y.Z --label NAME --output DIR [--formats tgz,deb,rpm]" >&2
            exit 2
            ;;
    esac
done

if [ -z "${build}" ] || [ -z "${label}" ] || [ -z "${version}" ] || [ -z "${output}" ]; then
    echo "package-tree.sh: --build, --version, --label and --output are required" >&2
    exit 2
fi
if ! [[ "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "package-tree.sh: --version must look like X.Y.Z" >&2
    exit 2
fi
case "${label}" in
    *[!A-Za-z0-9._+-]*) echo "package-tree.sh: unsafe --label" >&2; exit 2 ;;
esac

case "$(uname -s)" in
    Linux) platform=linux ;;
    Darwin) platform=macos ;;
    *) platform="$(uname -s | tr '[:upper:]' '[:lower:]')" ;;
esac
name="openrender-${label}-${platform}-$(uname -m)"

stage="$(mktemp -d)"
trap 'rm -rf "${stage}"' EXIT
cmake --install "${build}" --prefix "${stage}/opt/openrender"

export LD_LIBRARY_PATH="${stage}/opt/openrender/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export DYLD_LIBRARY_PATH="${stage}/opt/openrender/lib${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
"${stage}/opt/openrender/bin/orender" -h >/dev/null
"${stage}/opt/openrender/bin/orender" -v >/dev/null

IFS='.' read -r major minor patch <<<"${version}"
mkdir -p "${output}"

pack_one() {
    local generator="$1"
    local cfg
    cfg="$(mktemp)"
    cat >"${cfg}" <<EOF
set(CPACK_PACKAGE_NAME "openrender")
set(CPACK_PACKAGE_VENDOR "openRender")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "openRender ${label}")
set(CPACK_PACKAGE_VERSION_MAJOR "${major}")
set(CPACK_PACKAGE_VERSION_MINOR "${minor}")
set(CPACK_PACKAGE_VERSION_PATCH "${patch}")
set(CPACK_PACKAGE_VERSION "${version}")
set(CPACK_INSTALLED_DIRECTORIES "${stage};/")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_OUTPUT_FILE_PREFIX "${output}")
set(CPACK_PACKAGE_FILE_NAME "${name}")
EOF
    case "${generator}" in
        TGZ)
            echo 'set(CPACK_GENERATOR "TGZ")' >>"${cfg}"
            ;;
        DEB)
            cat >>"${cfg}" <<EOF
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME "${name}.deb")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "openRender maintainers")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
EOF
            ;;
        RPM)
            cat >>"${cfg}" <<EOF
set(CPACK_GENERATOR "RPM")
set(CPACK_RPM_FILE_NAME "${name}.rpm")
set(CPACK_RPM_PACKAGE_LICENSE "LGPL-2.1-only")
set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
EOF
            ;;
        *)
            echo "package-tree.sh: unknown format ${generator}" >&2
            exit 2
            ;;
    esac
    cpack --config "${cfg}"
    rm -f "${cfg}"
}

IFS=',' read -r -a requested <<<"${formats}"
for fmt in "${requested[@]}"; do
    case "${fmt}" in
        tgz) pack_one TGZ ;;
        deb) pack_one DEB ;;
        rpm) pack_one RPM ;;
        *) echo "package-tree.sh: unknown format ${fmt}" >&2; exit 2 ;;
    esac
done

ls -lh "${output}"
