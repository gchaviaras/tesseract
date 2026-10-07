#pragma once
#include "tk/host.h"
#include "tk/theme.h"
#include "views/GifController.h"
#include "views/GifPopup.h"
#include "views/MentionController.h"
#include "views/MentionPopup.h"
#include "views/ShortcodeController.h"
#include "views/ShortcodePopup.h"
#include "views/SlashCommandController.h"
#include "views/SlashCommandPopup.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tk
{
class TextArea;
}

namespace tesseract
{

class RoomPane;

namespace views
{
class RoomView;
}

// The composer's four autocomplete popups (@mention, /command, :shortcode:,
// /gif strip) for one room pane that lives in its own native window (the
// pop-out room windows on every platform): popup surface + popup widget +
// controller each, the hooks wiring via RoomPane, and the compose text-area
// callbacks that drive them through views/ComposePopups.h. It also sends the
// room's typing notice (start/stop) as the compose text goes non-empty/empty.
//
// Declare the owning shell's `std::unique_ptr<ComposerPopups>` AFTER the
// Surface/Host it was built from, so the popups are destroyed first. The
// controllers hold raw pointers to the text area and popup widgets.
class ComposerPopups
{
public:
    // Takes over text_area's on_changed / on_submit / popup-nav callbacks.
    // `host` is the pop-out surface's Host that the popup surfaces are made
    // from; pane / room_view / text_area must outlive this object.
    ComposerPopups(tk::Host& host, tk::TextArea* text_area, RoomPane* pane,
                   views::RoomView* room_view);
    ~ComposerPopups();

    ComposerPopups(const ComposerPopups&) = delete;
    ComposerPopups& operator=(const ComposerPopups&) = delete;

    // Close every popup (e.g. on window resize).
    void hide_all();

    // Position + show the /gif strip above the compose bar / hide it.
    void show_gif_popup();
    void hide_gif_popup();

    // Async GIF search results; only the controller that issued the request
    // matches, so every pop-out can forward every result.
    void on_gif_results(std::uint64_t request_id,
                        std::vector<GifResult> results);
    void on_gif_search_failed(std::uint64_t request_id,
                              const std::string& message);

    // Re-theme the popup surfaces (all four popup surfaces).
    void apply_theme(const tk::Theme& theme);

    // Animation tick: repaint the /gif strip when it is on screen.
    void repaint_anim_frame();

private:
    void refresh_(tk::PopupSurfaceHandle* popup, bool layout = true);

    tk::TextArea* text_area_;
    RoomPane* pane_;
    views::RoomView* room_view_;
    bool typing_active_ = false; // typing-notice debounce

    std::unique_ptr<tk::PopupSurfaceHandle> mention_popup_;
    views::MentionPopup* mention_popup_widget_ = nullptr;
    std::unique_ptr<views::MentionController> mention_controller_;

    std::unique_ptr<tk::PopupSurfaceHandle> slash_popup_;
    views::SlashCommandPopup* slash_popup_widget_ = nullptr;
    std::unique_ptr<views::SlashCommandController> slash_controller_;

    std::unique_ptr<tk::PopupSurfaceHandle> shortcode_popup_;
    views::ShortcodePopup* shortcode_popup_widget_ = nullptr;
    std::unique_ptr<views::ShortcodeController> shortcode_controller_;

    std::unique_ptr<tk::PopupSurfaceHandle> gif_popup_;
    views::GifPopup* gif_popup_widget_ = nullptr;
    std::unique_ptr<views::GifController> gif_controller_;
};

} // namespace tesseract
