#include "AppearanceSection.h"

#include "FontsEditor.h"
#include "MessageLayoutPreview.h"
#include "RoomListPreview.h"
#include "SettingsGroup.h"

#include "tesseract/settings.h"
#include "tk/i18n.h"
#include "tk/radio.h"
#include "tk/theme.h"
#include "tk/widget.h"

#include <tesseract/visual.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace tesseract::views
{

namespace
{

// One-line explainer shown under the message-layout combobox, tracking the
// current selection.
std::string message_layout_description(tesseract::Settings::MessageLayout layout)
{
    using ML = tesseract::Settings::MessageLayout;
    switch (layout)
    {
    case ML::Bubbles:
        return tk::tr("Your own messages appear on the right, in a subtle "
                      "rounded bubble.");
    case ML::Irc:
        return tk::tr("A monospaced, mIRC-style timeline: every line is "
                      "\"[time] <nick>\" with a colour per person.");
    case ML::Classic:
        break;
    }
    return tk::tr("Every message is flush left, with the sender's "
                  "avatar in the gutter.");
}

const char* message_layout_value(tesseract::Settings::MessageLayout layout)
{
    using ML = tesseract::Settings::MessageLayout;
    switch (layout)
    {
    case ML::Bubbles: return "bubbles";
    case ML::Irc:     return "irc";
    case ML::Classic: break;
    }
    return "classic";
}

tesseract::Settings::MessageLayout message_layout_from_value(const std::string& v)
{
    using ML = tesseract::Settings::MessageLayout;
    if (v == "bubbles") return ML::Bubbles;
    if (v == "irc")     return ML::Irc;
    return ML::Classic;
}

// Persisted value strings match tk::AccentTheme <-> Settings::ThemeAccent 1:1;
// keep them in sync with Settings::save_to_disk/load_from_disk's own strings.
const char* accent_value(tesseract::Settings::ThemeAccent accent)
{
    using TA = tesseract::Settings::ThemeAccent;
    switch (accent)
    {
    case TA::Blue:   return "blue";
    case TA::Forest: return "forest";
    case TA::Sunset: return "sunset";
    case TA::Violet: return "violet";
    case TA::System: break;
    }
    return "system";
}

tesseract::Settings::ThemeAccent accent_from_value(const std::string& v)
{
    using TA = tesseract::Settings::ThemeAccent;
    if (v == "blue")   return TA::Blue;
    if (v == "forest") return TA::Forest;
    if (v == "sunset") return TA::Sunset;
    if (v == "violet") return TA::Violet;
    return TA::System;
}

const char* accent_theme_value(tk::AccentTheme accent)
{
    switch (accent)
    {
    case tk::AccentTheme::Blue:   return "blue";
    case tk::AccentTheme::Forest: return "forest";
    case tk::AccentTheme::Sunset: return "sunset";
    case tk::AccentTheme::Violet: return "violet";
    case tk::AccentTheme::System: break;
    }
    return "system";
}

constexpr float kLayoutRowGap = 16.0f; // gap between the combo/desc column and the preview
constexpr float kLayoutColumnGap = 6.0f; // matches SettingsGroup's own inter-row spacing

const char* theme_value(tesseract::Settings::ThemePreference pref)
{
    using TP = tesseract::Settings::ThemePreference;
    switch (pref)
    {
    case TP::Light: return "light";
    case TP::Dark:  return "dark";
    case TP::System: break;
    }
    return "system";
}

tesseract::Settings::ThemePreference theme_from_value(const std::string& v)
{
    using TP = tesseract::Settings::ThemePreference;
    if (v == "light") return TP::Light;
    if (v == "dark")  return TP::Dark;
    return TP::System;
}

} // namespace


// ---------------------------------------------------------------------------
// AppearanceSection — thin SettingsPage with one "Theme" group.
// ---------------------------------------------------------------------------

AppearanceSection::AppearanceSection()
{
    auto* group = add_group(tk::tr("Theme"));
    auto picker = tk::create_widget<tk::RadioGroup>(
        this, tk::FlexBox::Axis::Horizontal);
    picker->set_spacing(kLayoutRowGap);
    picker->set_options({
        {tk::tr("Light"), "light"},
        {tk::tr("Dark"), "dark"},
        {tk::tr("System"), "system"},
    });
    picker->set_selected_value(
        theme_value(tesseract::Settings::instance().theme_pref));
    picker->on_changed = [this](std::string v)
    {
        if (on_theme_changed) on_theme_changed(theme_from_value(v));
    };
    picker_ = group->add_widget(std::move(picker));

    group->add_widget(tk::create_widget<tk::Label>(this, tk::tr("Accent color")));
    {
        auto accent_combo = tk::create_widget<tk::ComboBox>(this);
        std::vector<tk::ComboBox::Option> opts;
        for (const auto& info : tk::accent_theme_infos())
            opts.push_back({tk::tr(info.name), accent_theme_value(info.id)});
        accent_combo->set_options(std::move(opts));
        accent_combo->set_selected_value(
            accent_value(tesseract::Settings::instance().theme_accent));
        accent_combo->on_changed = [this](std::string v)
        {
            if (on_accent_changed) on_accent_changed(accent_from_value(v));
        };
        accent_combo_ = group->add_widget(std::move(accent_combo));
    }

    {
        using ML = tesseract::Settings::MessageLayout;
        const auto& s = tesseract::Settings::instance();
        auto* layout_group = add_group(tk::tr("Layout"));

        layout_group->add_widget(tk::create_widget<tk::Label>(
            this, tk::tr("Message layout")));

        // Combo + description on the left, live preview on the right.
        auto row = tk::create_widget<tk::HBox>(this);
        row->set_spacing(kLayoutRowGap);
        row->set_cross(tk::Cross::Start);

        auto column = tk::create_widget<tk::VBox>(this);
        column->set_layout_hints({.fill_main = true});
        column->set_spacing(kLayoutColumnGap);

        auto combo = tk::create_widget<tk::ComboBox>(this);
        combo->set_options({
            {tk::tr("Classic"), "classic"},
            {tk::tr("Bubbles"), "bubbles"},
            {tk::tr("IRC"), "irc"},
        });
        combo->set_selected_value(message_layout_value(s.message_layout));
        message_layout_combo_ = column->add_child(std::move(combo));

        auto desc = tk::create_widget<tk::Label>(
            this, message_layout_description(s.message_layout),
            tk::FontRole::Small);
        desc->set_wrap(true);
        message_layout_desc_ = column->add_child(std::move(desc));

        // Optional per-layout settings; each container is shown only while
        // its layout is selected.
        {
            auto opts = tk::create_widget<tk::VBox>(this);
            opts->set_spacing(kLayoutColumnGap);
            auto cb = tk::create_widget<tk::CheckButton>(
                this, tk::tr("Show my name and avatar"), s.bubbles_show_own_avatar);
            bubbles_own_avatar_cb_ = opts->add_child(std::move(cb));
            bubbles_own_avatar_cb_->on_change = [this](bool v)
            {
                if (on_bubbles_show_own_avatar_changed)
                    on_bubbles_show_own_avatar_changed(v);
            };
            opts->set_visible(s.message_layout == ML::Bubbles);
            layout_options_bubbles_ = column->add_child(std::move(opts));
        }

        row->add_child(std::move(column));
        message_layout_preview_ = row->add_child(
            std::make_unique<MessageLayoutPreview>());

        layout_group->add_widget(std::move(row));

        message_layout_combo_->on_changed = [this](std::string v)
        {
            const ML layout = message_layout_from_value(v);
            if (message_layout_desc_)
                message_layout_desc_->set_text(message_layout_description(layout));
            if (layout_options_bubbles_)
                layout_options_bubbles_->set_visible(layout == ML::Bubbles);
            if (on_message_layout_changed)
                on_message_layout_changed(layout);
            if (message_layout_preview_)
                message_layout_preview_->refresh_layout();
        };

        // Room list density: radio group + description on the left, live
        // preview on the right.
        layout_group->add_widget(tk::create_widget<tk::Label>(
            this, tk::tr("Room list")));

        auto density_row = tk::create_widget<tk::HBox>(this);
        density_row->set_spacing(kLayoutRowGap);
        density_row->set_cross(tk::Cross::Start);

        auto density_col = tk::create_widget<tk::VBox>(this);
        density_col->set_layout_hints({.fill_main = true});
        density_col->set_spacing(kLayoutColumnGap);

        auto density = tk::create_widget<tk::RadioGroup>(
            this, tk::FlexBox::Axis::Vertical);
        density->set_options({
            {tk::tr("Normal"), "normal"},
            {tk::tr("Compact"), "compact"},
        });
        density->set_selected_value(
            s.room_list_density == tesseract::Settings::RoomListDensity::Compact
                ? "compact"
                : "normal");
        room_list_density_group_ = density_col->add_child(std::move(density));

        auto density_desc = tk::create_widget<tk::Label>(
            this,
            tk::tr("Compact shows only room names, with smaller avatars."),
            tk::FontRole::Small);
        density_desc->set_wrap(true);
        density_col->add_child(std::move(density_desc));

        density_row->add_child(std::move(density_col));
        room_list_preview_ = density_row->add_child(
            std::make_unique<RoomListPreview>());
        room_list_preview_->set_compact(
            s.room_list_density == tesseract::Settings::RoomListDensity::Compact);
        layout_group->add_widget(std::move(density_row));

        room_list_density_group_->on_changed = [this](std::string v)
        {
            const bool compact = v == "compact";
            if (on_room_list_density_changed)
                on_room_list_density_changed(
                    compact ? tesseract::Settings::RoomListDensity::Compact
                            : tesseract::Settings::RoomListDensity::Normal);
            if (room_list_preview_)
                room_list_preview_->set_compact(compact);
        };
    }

    {
        auto* fonts_group = add_group(tk::tr("Fonts"));
        auto editor = tk::create_widget<FontsEditor>(this);
        editor->on_changed = [this](const tesseract::Settings::FontPrefs& p)
        {
            if (on_font_prefs_changed) on_font_prefs_changed(p);
        };
        fonts_editor_ = fonts_group->add_widget(std::move(editor));
    }

    {
        const auto& s = tesseract::Settings::instance();
        auto* rl_group = add_group(tk::tr("Room list"));

        auto cb_unread = tk::create_widget<tk::CheckButton>(
            this, tk::tr("Group unread rooms"), s.group_unread_rooms);
        group_unread_cb_ = rl_group->add_widget(std::move(cb_unread));
        group_unread_cb_->on_change = [this](bool v)
        {
            if (on_group_unread_changed) on_group_unread_changed(v);
        };

        auto cb = tk::create_widget<tk::CheckButton>(
            this, tk::tr("Group inactive rooms"), s.group_inactive_rooms);
        group_inactive_cb_ = rl_group->add_widget(std::move(cb));
        group_inactive_cb_->on_change = [this](bool v)
        {
            if (on_group_inactive_changed) on_group_inactive_changed(v);
        };

        auto combo = tk::create_widget<tk::ComboBox>(this);
        combo->set_options({
            {tk::tr("1 week"),   "7"},
            {tk::tr("2 weeks"),  "14"},
            {tk::tr("1 month"),  "30"},
            {tk::tr("3 months"), "90"},
            {tk::tr("6 months"), "180"},
        });
        combo->set_selected_value(
            std::to_string(s.inactive_room_threshold_days));
        period_combo_ = rl_group->add_widget(std::move(combo));
        period_combo_->on_changed = [this](std::string value)
        {
            if (on_inactive_period_changed)
            {
                on_inactive_period_changed(std::stoi(value));
            }
        };

        auto autoscroll = tk::create_widget<tk::CheckButton>(
            this, tk::tr("Scroll to rooms with new messages"), s.autoscroll_unread_rooms);
        autoscroll_cb_ = rl_group->add_widget(std::move(autoscroll));
        autoscroll_cb_->on_change = [this](bool v)
        {
            if (on_autoscroll_unread_changed) on_autoscroll_unread_changed(v);
        };
    }

    {
        const auto& s = tesseract::Settings::instance();
        auto* timeline_group = add_group(tk::tr("Timeline"));

        auto cb_membership = tk::create_widget<tk::CheckButton>(
            this, tk::tr("Show room join/leave events"), s.show_room_join_leave_events);
        show_membership_events_cb_ = timeline_group->add_widget(std::move(cb_membership));
        show_membership_events_cb_->on_change = [this](bool v)
        {
            if (on_show_membership_events_changed) on_show_membership_events_changed(v);
        };

        auto cb_anim_avatars = tk::create_widget<tk::CheckButton>(
            this, tk::tr("Animate avatars"), s.animate_avatars);
        animate_avatars_cb_ = timeline_group->add_widget(std::move(cb_anim_avatars));
        animate_avatars_cb_->on_change = [this](bool v)
        {
            if (on_animate_avatars_changed) on_animate_avatars_changed(v);
        };

        auto cb_status = tk::create_widget<tk::CheckButton>(
            this, tk::tr("Show status emoji next to names"),
            s.show_sender_status_in_timeline);
        show_sender_status_cb_ = timeline_group->add_widget(std::move(cb_status));
        show_sender_status_cb_->on_change = [this](bool v)
        {
            if (on_show_sender_status_changed) on_show_sender_status_changed(v);
        };
    }
}

AppearanceSection::~AppearanceSection() = default;

void AppearanceSection::set_selected(tesseract::Settings::ThemePreference pref)
{
    picker_->set_selected_value(theme_value(pref));
}

void AppearanceSection::set_selected_accent(tesseract::Settings::ThemeAccent accent)
{
    if (accent_combo_) accent_combo_->set_selected_value(accent_value(accent));
}

void AppearanceSection::set_group_unread(bool enabled)
{
    if (group_unread_cb_) group_unread_cb_->set_checked(enabled);
}

void AppearanceSection::set_group_inactive(bool enabled)
{
    if (group_inactive_cb_) group_inactive_cb_->set_checked(enabled);
}

void AppearanceSection::set_inactive_period(int days)
{
    if (period_combo_) period_combo_->set_selected_value(std::to_string(days));
}

void AppearanceSection::set_autoscroll_unread(bool enabled)
{
    if (autoscroll_cb_) autoscroll_cb_->set_checked(enabled);
}

void AppearanceSection::set_fonts(const tesseract::Settings::FontPrefs& prefs)
{
    if (fonts_editor_)
        fonts_editor_->set_prefs(prefs);
}

void AppearanceSection::set_message_layout(tesseract::Settings::MessageLayout layout)
{
    if (message_layout_combo_)
        message_layout_combo_->set_selected_value(message_layout_value(layout));
    if (message_layout_desc_)
        message_layout_desc_->set_text(message_layout_description(layout));
    if (layout_options_bubbles_)
        layout_options_bubbles_->set_visible(
            layout == tesseract::Settings::MessageLayout::Bubbles);
    if (message_layout_preview_)
        message_layout_preview_->refresh_layout();
}

void AppearanceSection::set_bubbles_show_own_avatar(bool enabled)
{
    if (bubbles_own_avatar_cb_) bubbles_own_avatar_cb_->set_checked(enabled);
}

void AppearanceSection::set_show_membership_events(bool enabled)
{
    if (show_membership_events_cb_) show_membership_events_cb_->set_checked(enabled);
}

void AppearanceSection::set_show_sender_status(bool enabled)
{
    if (show_sender_status_cb_) show_sender_status_cb_->set_checked(enabled);
}

void AppearanceSection::set_animate_avatars(bool enabled)
{
    if (animate_avatars_cb_) animate_avatars_cb_->set_checked(enabled);
}

} // namespace tesseract::views
