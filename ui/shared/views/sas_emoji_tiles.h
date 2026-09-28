#pragma once

// SAS comparison tiles (Matrix short authentication string): the three
// `decimal` numbers as one row of tiles, and the 7-emoji grid painted as 4
// tiles over 3 — the arrangement other Matrix clients use, so the user can
// compare row by row against the other device.

#include "tk/canvas.h"
#include "tk/widget.h"

#include <tesseract/types.h>

#include <array>
#include <cstdint>
#include <vector>

namespace tesseract::views
{

// Height the decimal row occupies (independent of width).
float sas_decimal_row_height();

// Paint the three SAS numbers as equal-width tiles across `area`.
void paint_sas_decimal_row(tk::PaintCtx& ctx, tk::Rect area,
                           const std::array<uint16_t, 3>& decimals);

// Height the grid occupies (independent of width).
float sas_emoji_grid_height();

// Paint `emojis` (normally exactly 7) into `area`, top-aligned.
void paint_sas_emoji_grid(tk::PaintCtx& ctx, tk::Rect area,
                          const std::vector<VerificationEmoji>& emojis);

} // namespace tesseract::views
