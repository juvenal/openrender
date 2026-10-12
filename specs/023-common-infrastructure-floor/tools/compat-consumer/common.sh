# Spec 023 compat-consumer helpers, sourced by the other scripts.
# shellcheck shell=bash

# prefix_of <install-root>: the install prefix inside a DESTDIR root (the
# directory holding include/ri.h), so scripts work for any configured prefix.
prefix_of() {
    local hit
    hit=$(find "$1" -path '*/include/ri.h' -print -quit 2>/dev/null)
    if [ -z "$hit" ]; then
        echo "no include/ri.h under $1" >&2
        return 1
    fi
    dirname "$(dirname "$hit")"
}

case "$(uname -s)" in
    Darwin) SHLIB_EXT=dylib ;;
    *) SHLIB_EXT=so ;;
esac

# shlib <prefix> <name>: the versioned shared library a consumer links against.
shlib() {
    if [ "$SHLIB_EXT" = dylib ]; then
        echo "$1/lib/lib$2.1.dylib"
    else
        echo "$1/lib/lib$2.so.1"
    fi
}

# runtime_env <prefix>: renderer search paths for the installed tree. Library
# search-path variables are deliberately NOT set here (see run.sh --pinned).
runtime_env() {
    if [ -d "$1/share/openRender/shaders" ]; then
        # FHS layout (INSTALL_SELFCONTAINED=OFF, the Linux default)
        export DISPLAYS="$1/lib/openRender/displays"
        export SHADERS="$1/share/openRender/shaders"
        export ORENDERHOME="$1/share/openRender"
    else
        export DISPLAYS="$1/displays"
        export SHADERS="$1/shaders"
        export ORENDERHOME="$1"
    fi
}

# pin_libs <prefix>: force the loader to the install's lib directory. A
# diagnostic aid only; the compatibility proof runs without it.
pin_libs() {
    export DYLD_LIBRARY_PATH="$1/lib"
    export LD_LIBRARY_PATH="$1/lib"
}

# check_resolution <root> <binary> [args...]: run the binary once with the
# loader's library tracing on, and fail if any libRIB*/libshader_* resolves
# outside <root>.
check_resolution() {
    local root="$1"
    shift
    local libs
    if [ "$SHLIB_EXT" = dylib ]; then
        libs=$(DYLD_PRINT_LIBRARIES=1 "$@" 2>&1 >/dev/null | sed -n 's/^dyld\[[0-9]*\]: <[^>]*> //p')
    else
        libs=$(ldd "$1" | awk '{print $3}')
    fi
    local bad
    bad=$(printf '%s\n' "$libs" | grep -E 'lib(RIB|shader_)' | grep -v "^$root" || true)
    printf '%s\n' "$libs" | grep -E 'lib(RIB|shader_)' | sed 's/^/    resolved: /'
    if [ -n "$bad" ]; then
        echo "    ERROR: resolved outside $root:" >&2
        printf '      %s\n' $bad >&2
        return 1
    fi
}
