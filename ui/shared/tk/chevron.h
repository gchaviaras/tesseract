#pragma once

// A drawn "expand" chevron (a ▾ shape) for dropdown controls. Vector lines,
// not the U+25BE glyph: a text glyph falls back to whatever symbol or emoji
// font the platform has and looks different on every backend.

#include "tk/canvas.h"

#include <algorithm>

namespace tk
{

// Draws a down-pointing chevron centred in `cell`.
inline void draw_chevron_down(Canvas& canvas, Rect cell, Color color)
{
    const float half_w = std::clamp(cell.w * 0.18f, 3.0f, 5.0f);
    const float half_h = half_w * 0.55f;
    const float cx = cell.x + cell.w * 0.5f;
    const float cy = cell.y + cell.h * 0.5f;
    const Point left{cx - half_w, cy - half_h};
    const Point mid{cx, cy + half_h};
    const Point right{cx + half_w, cy - half_h};
    canvas.draw_line(left, mid, color, 1.5f);
    canvas.draw_line(mid, right, color, 1.5f);
}

} // namespace tk
