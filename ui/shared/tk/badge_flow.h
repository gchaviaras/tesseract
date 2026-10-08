#pragma once

// A wrapping row of "badges": each is a small leading icon (an embedded SVG,
// or a live image such as a bridged network's logo) followed by a one-line
// muted label. Badges flow left-to-right and wrap onto further rows when the
// next one would not fit, so a narrow panel never clips or overflows them;
// a single badge wider than the row has its label ellipsized instead.

#include "canvas.h"
#include "svg.h"
#include "widget.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace tk
{

class BadgeFlow : public Widget
{
protected:
    BadgeFlow() = default;
    TK_WIDGET_FACTORY_FRIEND(BadgeFlow)

public:
    struct Badge
    {
        // Already-translated label text.
        std::string label;
        // Leading glyph (embedded SVG bytes; must outlive the widget).
        std::span<const std::uint8_t> icon;
        // Optional live image drawn as a circle in place of `icon` when it
        // returns non-null (e.g. a bridged network's logo while it loads).
        std::function<const Image*()> image;
    };

    // Replaces the badges and schedules a relayout when anything changed.
    void set_badges(std::vector<Badge> badges);

    // Wraps to constraints.w; returns {widest row, total height}. Zero height
    // when there are no badges.
    Size measure(LayoutCtx&, Size constraints) override;
    void paint(PaintCtx&) override;

    Role access_role() const override
    {
        return badges_.empty() ? Role::None : Role::StaticText;
    }
    // Labels joined with ", " (already localized by the caller).
    std::string access_name() const override;

    // Spacing, exposed for callers that reserve a minimum row height.
    static constexpr float kIconPx = 14.0f;
    static constexpr float kIconGap = 4.0f;
    static constexpr float kBadgeGap = 12.0f;
    static constexpr float kRowGap = 4.0f;

private:
    struct Placed
    {
        float x = 0;
        float y = 0;
    };

    // Lays every badge out for `width`; cached until the width, badge list
    // or text metrics change.
    Size flow_(CanvasFactory& factory, float width);

    std::vector<Badge> badges_;
    std::vector<IconCache> icons_;
    std::vector<std::unique_ptr<TextLayout>> layouts_;
    std::vector<Placed> placed_;
    float row_h_ = 16.0f;
    float flow_width_ = -1.0f;
    Size flow_size_{};
};

} // namespace tk
