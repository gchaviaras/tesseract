#pragma once

// Settings panel section: network proxy (system / none / manual URL).
// Applied at launch, so a change takes effect after restart.

#include "SettingBinder.h"
#include "SettingsPage.h"
#include "tk/combobox.h"
#include "tk/controls.h"
#include "tk/text_field.h"
#include "tesseract/settings.h"

#include <functional>
#include <string>

namespace tesseract::views
{

class NetworkSection : public SettingsPage
{
public:
    explicit NetworkSection(SettingBinder& binder);
    ~NetworkSection() override = default;

    // Show the "Restart now" row — set while the saved proxy differs from the
    // one this process started with.
    void set_restart_pending(bool pending);

    // On a user change of the mode, or of the URL in Manual mode, the new
    // values are written to Settings and SettingKey::Proxy is reported. An
    // invalid Manual URL never reports (an inline error is shown instead).

    // Fires when the user presses "Restart now".
    std::function<void()> on_restart_requested;

private:
    void load_();
    void commit_(tesseract::Settings::ProxyMode mode, const std::string& url);
    void on_mode_changed_(const std::string& value);
    void on_url_edited_();
    void refresh_url_state_();
    tesseract::Settings::ProxyMode mode_() const;

    SettingBinder* binder_ = nullptr;
    bool loading_ = false;
    tk::ComboBox* combo_ = nullptr;
    tk::TextField* url_field_ = nullptr;
    tk::Label* error_label_ = nullptr;
    tk::Widget* restart_row_ = nullptr;
};

} // namespace tesseract::views
