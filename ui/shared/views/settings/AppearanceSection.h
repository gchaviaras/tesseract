#pragma once

// Settings panel section: theme selection + room-list grouping options.
// A SettingsPage with a "Theme" group (Light/Dark/System radio group) and a
// "Room list" group (checkbox + period combobox).

#include "SettingsPage.h"

#include "tesseract/settings.h"
#include "tk/combobox.h"
#include "tk/controls.h"
#include "tk/radio.h"

#include <functional>

namespace tesseract::views
{

class FontsEditor;
class MessageLayoutPreview;
class RoomListPreview;

class AppearanceSection : public SettingsPage
{
public:
    AppearanceSection();
    ~AppearanceSection() override;

    // Silently update the displayed selection without firing on_theme_changed.
    void set_selected(tesseract::Settings::ThemePreference pref);

    // Fires with the newly selected preference when the user picks a button.
    std::function<void(tesseract::Settings::ThemePreference)> on_theme_changed;

    // Silently update the displayed accent selection without firing on_accent_changed.
    void set_selected_accent(tesseract::Settings::ThemeAccent accent);

    // Fires with the newly selected accent when the user picks one in the combobox.
    std::function<void(tesseract::Settings::ThemeAccent)> on_accent_changed;

    // ----- Room list group -----
    // Silently update the controls without firing the callbacks below.
    void set_group_unread(bool enabled);
    void set_group_inactive(bool enabled);
    void set_inactive_period(int days);
    void set_autoscroll_unread(bool enabled);

    // Fired when the user toggles grouping / changes the period / toggles
    // auto-scroll-to-unread.
    std::function<void(bool)> on_group_unread_changed;
    std::function<void(bool)> on_group_inactive_changed;
    std::function<void(int)>  on_inactive_period_changed;
    std::function<void(bool)> on_autoscroll_unread_changed;

    // ----- Timeline group -----
    // Silently update the control without firing on_show_membership_events_changed.
    void set_show_membership_events(bool enabled);

    // Fired when the user toggles the "show join/leave events" checkbox.
    std::function<void(bool)> on_show_membership_events_changed;

    // Silently update the control without firing on_show_sender_status_changed.
    void set_show_sender_status(bool enabled);

    // Fired when the user toggles the "show status emoji next to names" checkbox.
    std::function<void(bool)> on_show_sender_status_changed;

    // Silently update the control without firing on_animate_avatars_changed.
    void set_animate_avatars(bool enabled);

    // Fired when the user toggles the "animate avatars" checkbox.
    std::function<void(bool)> on_animate_avatars_changed;

    // ----- Layout group -----
    // Silently update the message-layout combobox.
    void set_message_layout(tesseract::Settings::MessageLayout layout);

    // Fired when the user picks a layout in the combobox.
    std::function<void(tesseract::Settings::MessageLayout)> on_message_layout_changed;

    // Silently update the Bubbles "show my avatar" checkbox.
    void set_bubbles_show_own_avatar(bool enabled);

    // Fired when the user toggles the Bubbles "show my avatar" checkbox.
    std::function<void(bool)> on_bubbles_show_own_avatar_changed;

    // Fired when the user picks Normal / Compact for the room list.
    std::function<void(tesseract::Settings::RoomListDensity)> on_room_list_density_changed;

    // ----- Fonts group -----
    // Silently update the controls without firing on_font_prefs_changed.
    void set_fonts(const tesseract::Settings::FontPrefs& prefs);

    // Fired after every font edit with the full new preferences.
    std::function<void(const tesseract::Settings::FontPrefs&)> on_font_prefs_changed;

private:
    tk::RadioGroup*  picker_             = nullptr;
    tk::ComboBox*    accent_combo_       = nullptr;
    tk::CheckButton* group_unread_cb_    = nullptr;
    tk::CheckButton* group_inactive_cb_  = nullptr;
    tk::ComboBox*    period_combo_       = nullptr;
    tk::CheckButton* autoscroll_cb_      = nullptr;
    tk::CheckButton* show_membership_events_cb_ = nullptr;
    tk::CheckButton* show_sender_status_cb_     = nullptr;
    tk::CheckButton* animate_avatars_cb_        = nullptr;
    tk::ComboBox*    message_layout_combo_      = nullptr;
    tk::Label*       message_layout_desc_       = nullptr;
    MessageLayoutPreview* message_layout_preview_ = nullptr;
    tk::VBox*        layout_options_bubbles_    = nullptr; // per-layout options: Bubbles
    tk::CheckButton* bubbles_own_avatar_cb_     = nullptr;
    tk::RadioGroup*       room_list_density_group_ = nullptr;
    RoomListPreview*      room_list_preview_       = nullptr;
    FontsEditor*          fonts_editor_            = nullptr;
};

} // namespace tesseract::views
