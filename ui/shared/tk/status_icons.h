#pragma once
#include <cstdint>
#include <span>

// Accessors for the handful of Lucide icon documents needed outside the files
// that #include the generated icons.h: native platform shells painting outside
// any tk::Canvas surface (status-bar glyphs and the like), and tk widgets that
// stay off icons.h (see the unity-build note in ui/shared/CMakeLists.txt).
// Rasterize the bytes with tk::rasterize_svg / tk::rasterize_svg_rgba.

namespace tk
{

// Lucide "battery-low" (currentColor line icon) — the low-power-mode
// status-bar indicator.
std::span<const std::uint8_t> low_power_icon_svg();

// Lucide "check" — checkbox ticks and selected-item marks.
std::span<const std::uint8_t> check_icon_svg();

} // namespace tk
