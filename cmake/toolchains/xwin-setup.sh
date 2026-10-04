#!/usr/bin/env bash
# One-time setup for the windows-xwin-* presets (Win32 cross-compile from
# Linux, MSVC ABI). See cmake/toolchains/xwin-x86_64.cmake and docs/BUILD.md.
#
# Downloads the Microsoft CRT + Windows SDK via xwin (~1.1 GB download cache,
# ~650 MB sysroot). By doing so you accept Microsoft's license terms for
# those components, which is why --accept-license must be passed explicitly.
#
# Usage: cmake/toolchains/xwin-setup.sh --accept-license
# Root defaults to ~/.cache/xwin; override with XWIN_ROOT.
set -euo pipefail

if [[ "${1:-}" != "--accept-license" ]]; then
    echo "usage: $0 --accept-license" >&2
    echo "Downloads the MSVC CRT and Windows SDK; you must accept Microsoft's license" >&2
    echo "(https://go.microsoft.com/fwlink/?LinkId=2086102) to proceed." >&2
    exit 2
fi

root="${XWIN_ROOT:-$HOME/.cache/xwin}"

for tool in clang-cl lld-link llvm-lib llvm-rc llvm-mt; do
    command -v "$tool" >/dev/null || { echo "missing $tool (install clang + lld + llvm)" >&2; exit 1; }
done

rustup target add x86_64-pc-windows-msvc

xwin_bin="$(command -v xwin || true)"
if [[ -z "$xwin_bin" ]]; then
    cargo install xwin --locked
    xwin_bin="${CARGO_HOME:-$HOME/.cargo}/bin/xwin"
fi

"$xwin_bin" --accept-license --cache-dir "$root/cache" splat --output "$root/splat"

# xwin lowercases every header and only adds mixed-case symlinks for names it
# sees #included by other SDK headers. C++/WinRT projection headers we include
# directly (e.g. <winrt/Windows.Foundation.Collections.h>) can be missing; the
# proper casing survives in impl/<Name>.0.h, so recreate the top-level links.
winrt="$root/splat/sdk/include/cppwinrt/winrt"
for f in "$winrt"/impl/*.0.h; do
    name="$(basename "$f" .0.h)"
    [[ "$name" =~ [A-Z] ]] || continue
    lower="${name,,}"
    if [[ ! -e "$winrt/$name.h" && -e "$winrt/$lower.h" ]]; then
        ln -s "$lower.h" "$winrt/$name.h"
    fi
done

# Host Python for the Windows .ico generator (ui/icons/generate_ico.py).
if [[ ! -x "$root/venv/bin/python3" ]]; then
    python3 -m venv "$root/venv"
fi
"$root/venv/bin/pip" install --quiet resvg-py

echo "xwin sysroot ready at $root/splat"
echo "  cmake --preset windows-xwin-debug && cmake --build build/windows-xwin-debug"
