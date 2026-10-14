#pragma once
// HoverArea — an invisible clickable region for content its owner paints
// itself (an avatar, a chip, a title block). It gives that region the
// standard widget hover feedback, pointer cursor, press/click handling and
// (optionally) a keyboard/accessibility path, without the owner keeping any
// rect, press flag or hit-test of its own:
//
//   area_ = add_child(tk::create_widget<tk::HoverArea>(this));
//   area_->set_on_click([this] { ... });
//   // each layout pass: area_->arrange(lc, rect_or_empty);
//   // each paint, after the owner's own content: area_->paint(ctx);
//
// By default it is pointer-only: owners that already place a
// tk::KeyboardTarget over the same area keep it as the one Tab stop. Call
// set_pointer_only(false) to make the area itself focusable (Enter/Space,
// Role::Button — also set an accessible name through the subclass or use a
// tk::Button instead).
//
// Paints only the hover feedback (Widget::paint_hover_fill) — never children.
// An empty (or never-arranged) rect cannot be hit, so "not clickable right
// now" is expressed by arranging it with {}.
#include "widget.h"

namespace tk
{

class HoverArea : public Widget
{
public:
    explicit HoverArea(HoverStyle style = HoverStyle::Fill, float radius = 6.0f)
    {
        set_hoverable(true, style);
        set_hover_radius(radius);
        set_pointer_only(true);
    }

    Size measure(LayoutCtx&, Size) override
    {
        return {bounds_.w, bounds_.h};
    }

    void paint(PaintCtx& ctx) override
    {
        paint_hover_fill(ctx);
    }

    // For owners that only learn the rect while painting (the room chip
    // strips): place the area without a LayoutCtx. An empty rect parks it.
    void set_rect(Rect r)
    {
        bounds_ = r;
    }
};

} // namespace tk
