#include "NetworkSection.h"

#include "SettingsGroup.h"
#include "tk/i18n.h"
#include "tk/layout.h"
#include "tesseract/client.h"

namespace tesseract::views
{

using ProxyMode = tesseract::Settings::ProxyMode;

namespace
{
const char* mode_value(ProxyMode m)
{
    return m == ProxyMode::None ? "none" : m == ProxyMode::Manual ? "manual" : "system";
}
} // namespace

NetworkSection::NetworkSection()
{
    auto* group = add_group(tk::tr("Proxy"));

    auto combo = tk::create_widget<tk::ComboBox>(this);
    combo->set_options({
        {tk::tr("Use system proxy"), "system"},
        {tk::tr("No proxy"),         "none"},
        {tk::tr("Manual"),           "manual"},
    });
    combo->set_selected_value(mode_value(tesseract::Settings::instance().proxy_mode));
    combo_ = group->add_widget(std::move(combo));
    combo_->on_changed = [this](std::string value) { on_mode_changed_(value); };

    auto url_row = tk::create_widget<tk::HBox>(this);
    url_row->set_spacing(8.0f).set_cross(tk::Cross::Center);
    url_row->add_child(tk::create_widget<tk::Label>(this, tk::tr("Proxy URL:")));

    auto field = tk::create_widget<tk::TextField>(this, 32.0f);
    field->set_placeholder(tk::tr("http://proxy.example.com:8080"));
    field->set_text(tesseract::Settings::instance().proxy_url);
    field->set_on_changed([this](const std::string&) { on_url_edited_(); });
    field->set_layout_hints({.fill_main = true});
    url_field_ = url_row->add_child(std::move(field));
    group->add_widget(std::move(url_row));

    auto help = tk::create_widget<tk::Label>(
        this,
        tk::tr("HTTP and HTTPS proxies. Calls' audio/video can't go through an HTTP proxy."),
        tk::FontRole::Small);
    group->add_widget(std::move(help));

    auto err = tk::create_widget<tk::Label>(
        this, tk::tr("Enter a valid http:// or https:// proxy URL."));
    error_label_ = group->add_widget(std::move(err));
    error_label_->set_visible(false);

    auto note = tk::create_widget<tk::Label>(
        this, tk::tr("Changes take effect after restart."), tk::FontRole::Small);
    group->add_widget(std::move(note));

    // HBox so the button keeps its natural width instead of stretching.
    auto row = tk::create_widget<tk::HBox>(this);
    row->add_child(tk::create_widget<tk::Button>(this,
        tk::tr("Restart now"),
        [this] { if (on_restart_requested) on_restart_requested(); }));
    row->set_visible(false);
    restart_row_ = group->add_widget(std::move(row));

    refresh_url_state_();
}

ProxyMode NetworkSection::mode_() const
{
    const std::string& v = combo_->selected_value();
    return v == "none" ? ProxyMode::None : v == "manual" ? ProxyMode::Manual : ProxyMode::System;
}

void NetworkSection::refresh_url_state_()
{
    const bool manual = mode_() == ProxyMode::Manual;
    url_field_->set_enabled(manual);
    const std::string url = url_field_->text();
    error_label_->set_visible(manual && !url.empty() && !tesseract::Client::is_valid_proxy_url(url));
}

void NetworkSection::on_mode_changed_(const std::string&)
{
    refresh_url_state_();
    const ProxyMode m = mode_();
    if (m != ProxyMode::Manual)
    {
        if (on_proxy_changed)
        {
            on_proxy_changed(m, url_field_->text());
        }
        return;
    }
    on_url_edited_();
}

void NetworkSection::on_url_edited_()
{
    refresh_url_state_();
    if (mode_() != ProxyMode::Manual)
    {
        return;
    }
    const std::string url = url_field_->text();
    if (!tesseract::Client::is_valid_proxy_url(url))
    {
        return;
    }
    if (on_proxy_changed)
    {
        on_proxy_changed(ProxyMode::Manual, url);
    }
}

void NetworkSection::set_proxy(ProxyMode mode, const std::string& url)
{
    // set_on_changed may fire on set_text; suppress by clearing the callback
    // target for the duration.
    auto cb = std::move(on_proxy_changed);
    on_proxy_changed = nullptr;
    combo_->set_selected_value(mode_value(mode));
    url_field_->set_text(url);
    on_proxy_changed = std::move(cb);
    refresh_url_state_();
}

void NetworkSection::set_restart_pending(bool pending)
{
    if (restart_row_)
    {
        restart_row_->set_visible(pending);
    }
}

} // namespace tesseract::views
