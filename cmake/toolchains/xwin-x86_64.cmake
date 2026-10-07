# Cross-compile the Win32 build from Linux against the MSVC ABI
# (x86_64-pc-windows-msvc) with clang-cl + lld-link, using a Microsoft CRT and
# Windows SDK downloaded and "splatted" by xwin. Prepare the sysroot once with
# cmake/toolchains/xwin-setup.sh; used by the windows-xwin-* presets.
#
# MSVC ABI rather than MinGW because webrtc-sys (calls) links LiveKit's prebuilt
# libwebrtc, which only ships MSVC-built — there is no GNU-ABI variant.
#
# This is a compile/link check: the resulting .exe is a real MSVC-ABI binary,
# but it is not a release artifact (CI builds those natively on Windows).

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_VERSION   10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(DEFINED ENV{XWIN_ROOT})
    set(_xwin_root_default "$ENV{XWIN_ROOT}")
else()
    set(_xwin_root_default "$ENV{HOME}/.cache/xwin")
endif()
set(TESSERACT_XWIN_ROOT "${_xwin_root_default}" CACHE PATH
    "xwin root (holds splat/ and, optionally, venv/) — see cmake/toolchains/xwin-setup.sh")
set(_xwin "${TESSERACT_XWIN_ROOT}/splat")
if(NOT EXISTS "${_xwin}/sdk/include/um/windows.h")
    message(FATAL_ERROR
        "No xwin sysroot at ${_xwin}. Run cmake/toolchains/xwin-setup.sh "
        "(or set XWIN_ROOT / -DTESSERACT_XWIN_ROOT=).")
endif()

set(CMAKE_C_COMPILER   clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER       lld-link)
set(CMAKE_AR           llvm-lib)
set(CMAKE_RC_COMPILER  llvm-rc)
set(CMAKE_MT           llvm-mt)

# Sysroot flags. Also forwarded to cc-rs for the Rust crates that compile C/C++
# (webrtc-sys, cxx, aws-lc-sys, bundled SQLite) — see the root CMakeLists.txt.
set(TESSERACT_MSVC_CROSS_CFLAGS
    "--target=x86_64-pc-windows-msvc /imsvc${_xwin}/crt/include /imsvc${_xwin}/sdk/include/ucrt /imsvc${_xwin}/sdk/include/um /imsvc${_xwin}/sdk/include/shared /imsvc${_xwin}/sdk/include/winrt /imsvc${_xwin}/sdk/include/cppwinrt")
set(CMAKE_C_FLAGS_INIT   "${TESSERACT_MSVC_CROSS_CFLAGS}")
# Diagnostics clang-cl reports but the native MSVC build doesn't (COM overrides
# without noexcept, CRT getenv/localtime deprecation,
# clang-cl's default -fdelayed-template-parsing). C++ only: not forwarded to cc-rs.
set(CMAKE_CXX_FLAGS_INIT "${TESSERACT_MSVC_CROSS_CFLAGS} -Wno-microsoft-exception-spec -Wno-delayed-template-parsing-in-cxx20 -Wno-deprecated-declarations")

set(_xwin_libpaths
    "/libpath:${_xwin}/crt/lib/x86_64 /libpath:${_xwin}/sdk/lib/um/x86_64 /libpath:${_xwin}/sdk/lib/ucrt/x86_64")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_xwin_libpaths}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_xwin_libpaths}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_xwin_libpaths}")

# xwin ships only the release CRT (no msvcrtd.lib), and the project links the
# static CRT anyway to match libwebrtc. Setting it here, before project(),
# makes CMake's own compiler checks use it too.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded CACHE STRING "")

set(CMAKE_FIND_ROOT_PATH "${_xwin}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(Rust_CARGO_TARGET x86_64-pc-windows-msvc CACHE STRING "")

# The Windows icon generator needs resvg-py on the host Python. xwin-setup.sh
# installs it into a venv next to the sysroot; prefer that when present.
if(EXISTS "${TESSERACT_XWIN_ROOT}/venv/bin/python3")
    set(Python3_EXECUTABLE "${TESSERACT_XWIN_ROOT}/venv/bin/python3" CACHE FILEPATH "")
endif()
