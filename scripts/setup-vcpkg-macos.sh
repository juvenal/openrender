#!/usr/bin/env bash
# One-time setup for the shared vcpkg checkout used by every local macOS
# build of openRender (not just CI/release) -- see vcpkg.json and
# CMake/vcpkg-triplets/*.cmake. Safe to re-run: it only updates the existing
# checkout to the pinned baseline instead of re-cloning.
#
# This script never touches your shell profile. It clones+bootstraps vcpkg
# and prints the exports you need to add yourself -- one shared checkout and
# one shared installed tree, so every worktree's build reuses the same
# compiled zlib/libpng/tiff/openexr instead of each worktree paying its own
# ~13MB install and separately populating its own binary cache.
set -euo pipefail

VCPKG_ROOT_DEFAULT="$HOME/.vcpkg"
VCPKG_INSTALLED_DIR_DEFAULT="$HOME/.cache/vcpkg-installed/openrender"

# Must match vcpkg.json's builtin-baseline exactly, or vcpkg will fetch the
# baseline commit's port versions but build against a differently-versioned
# vcpkg tool -- keep the two in lockstep by hand.
BASELINE_SHA="319504a5326aa870edde46438c5455fa76305a56"

# Both positional args are for testing this script against a scratch
# location; a normal one-time run takes neither and uses the shared defaults
# above for both.
VCPKG_ROOT="${1:-$VCPKG_ROOT_DEFAULT}"
VCPKG_INSTALLED_DIR="${2:-$VCPKG_INSTALLED_DIR_DEFAULT}"

if [[ -d "$VCPKG_ROOT/.git" ]]; then
    echo "Updating existing vcpkg checkout at $VCPKG_ROOT ..."
    git -C "$VCPKG_ROOT" fetch --quiet origin
else
    # git clone refuses a non-empty target directory, and $VCPKG_ROOT can
    # already be non-empty here with nothing to do with this script: the
    # vcpkg tool itself writes a per-user ~/.vcpkg/config (telemetry
    # opt-out state) the first time it's ever invoked by anyone, on any
    # project, regardless of VCPKG_ROOT -- a plain `git clone` into that
    # directory fails with "already exists and is not an empty directory"
    # even though there's no real checkout there yet. `git init` (unlike
    # `git clone`) doesn't care whether the directory is empty, so build
    # the checkout in place instead of cloning into a fresh one.
    echo "Setting up vcpkg checkout at $VCPKG_ROOT ..."
    mkdir -p "$VCPKG_ROOT"
    git -C "$VCPKG_ROOT" init --quiet
    git -C "$VCPKG_ROOT" remote add origin https://github.com/microsoft/vcpkg.git
    git -C "$VCPKG_ROOT" fetch --quiet origin
fi

git -C "$VCPKG_ROOT" checkout --quiet "$BASELINE_SHA"
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

mkdir -p "$VCPKG_INSTALLED_DIR"

cat <<EOF

vcpkg is ready at $VCPKG_ROOT (pinned to $BASELINE_SHA).

Add these to your shell profile (~/.zshrc or equivalent) to opt every local
build into vcpkg-vendored dependencies, shared across all worktrees:

    export VCPKG_ROOT="$VCPKG_ROOT"
    export VCPKG_INSTALLED_DIR="$VCPKG_INSTALLED_DIR"

This script does not edit your shell profile for you. Leaving VCPKG_ROOT
unset keeps using Homebrew exactly as before -- nothing else changes.

IMPORTANT: a build directory configured before you set these (or configured
under the other mode) will NOT pick them up on a plain re-configure --
CMAKE_TOOLCHAIN_FILE is only honored on a build directory's first configure,
and CMake silently accepts a later change to it without doing anything.
Delete and recreate the build directory (e.g. \`rm -rf build\`) whenever you
flip VCPKG_ROOT on or off for an existing worktree.

When you next bump vcpkg.json's builtin-baseline, re-run this script with no
running builds in any worktree, then update BASELINE_SHA at the top of this
file to match.
EOF
