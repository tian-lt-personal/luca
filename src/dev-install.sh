#!/usr/bin/env bash
# Dev install for LUCA: builds the CLI and the built-in library, installs them
# into a Unix prefix, and adds the bin directory to the user's PATH.
# Run: ./dev-install.sh [Debug|Release] [prefix]

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
configuration="${1:-Release}"
prefix="${2:-/usr/local/luca-cpp}"

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

usage() {
    printf 'Usage: %s [Debug|Release] [prefix]\n' "$(basename -- "$0")"
    printf 'Defaults: Release /usr/local/luca-cpp\n'
}

if [[ "$configuration" != Debug && "$configuration" != Release ]]; then
    usage >&2
    fail "configuration must be Debug or Release"
fi

# --- prerequisites, checked before any sudo prompt ---
[[ -n "${VCPKG_ROOT:-}" ]] || \
    fail "VCPKG_ROOT is not set; set it to your vcpkg checkout."
[[ -d "$VCPKG_ROOT" ]] || \
    fail "VCPKG_ROOT does not point to a directory: $VCPKG_ROOT"
command -v re2c >/dev/null 2>&1 || \
    fail "re2c was not found on PATH; install it with your distribution's package manager."
command -v cmake >/dev/null 2>&1 || \
    fail "cmake was not found on PATH; install it with your distribution's package manager."

build_type="$configuration"
build_dir="$script_dir/b_/ci-gcc"
bin_dir="$prefix/bin"

cd -- "$script_dir"

# ci-gcc is the single-configuration Linux/Ninja preset. The build type is
# supplied here because the preset is shared by CI and does not force one.
cmake --preset ci-gcc -DCMAKE_BUILD_TYPE="$build_type" || \
    fail "cmake configure failed (see output above)."
cmake --build --preset ci-gcc --target lucacli || \
    fail "cmake build failed (see output above)."

# Use sudo only when the selected prefix is not writable by the current user.
# This keeps custom user-owned prefixes completely unprivileged.
install_as_root=0
if [[ -e "$prefix" ]]; then
    [[ -w "$prefix" ]] || install_as_root=1
else
    prefix_parent="$(dirname -- "$prefix")"
    [[ -d "$prefix_parent" && -w "$prefix_parent" ]] || install_as_root=1
fi

if (( install_as_root )); then
    command -v sudo >/dev/null 2>&1 || \
        fail "cannot write '$prefix' and sudo was not found; choose a writable prefix or install sudo."
    sudo -v || fail "sudo authentication failed; nothing was installed."
    sudo cmake --install "$build_dir" --prefix "$prefix" || \
        fail "cmake install failed (see output above)."
else
    cmake --install "$build_dir" --prefix "$prefix" || \
        fail "cmake install failed (see output above)."
fi

# --- verify, then add the bin directory to the user's login-shell PATH ---
luca_bin="$bin_dir/luca"
[[ -x "$luca_bin" && -f "$bin_dir/std.luca" ]] || \
    fail "verification failed: '$bin_dir' is missing executable luca or std.luca."

profile_file="${HOME}/.profile"
path_line="export PATH=$(printf '%q' "$bin_dir"):\$PATH"
if [[ ! -f "$profile_file" ]] || ! grep -Fqx -- "$path_line" "$profile_file"; then
    {
        [[ ! -s "$profile_file" ]] || printf '\n'
        printf '%s\n' "$path_line"
    } >> "$profile_file"
    printf "Added '%s' to %s.\n" "$bin_dir" "$profile_file"
else
    printf "'%s' is already on the user PATH in %s.\n" "$bin_dir" "$profile_file"
fi

printf '\nLUCA installed:\n'
printf '  exe:  %s\n' "$luca_bin"
printf '  std:  %s\n' "$bin_dir/std.luca"
printf "Open a new login shell (or run 'source %s') to pick up the PATH, then run 'luca'.\n" "$profile_file"
