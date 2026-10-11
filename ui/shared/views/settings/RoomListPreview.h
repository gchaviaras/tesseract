#pragma once

// Small settings-panel widget: hosts a real (but inert — no providers, no live
// rooms) RoomListView fed three fake rooms, so the "Room list" density radio
// in AppearanceSection has a live preview of Normal/Compact next to it.

#include "tk/widget.h"

namespace tesseract::views
{

class RoomListView;

class RoomListPreview : public tk::Widget
{
public:
    RoomListPreview();

    // Switches the nested list between Normal and Compact rows.
    void set_compact(bool compact);

    tk::Size measure(tk::LayoutCtx&, tk::Size constraints) override;

private:
    RoomListView* list_ = nullptr;
};

} // namespace tesseract::views
