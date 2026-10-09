#pragma once

// Settings panel section: network proxy (system / none / manual URL).
// Applied at launch, so a change takes effect after restart.

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
    NetworkSection();
    ~NetworkSection() override = default;

    // Silently update the controls without firing on_proxy_changed.
    void set_proxy(tesseract::Settings::ProxyMode mode, const std::string& url);

    // Show the "Restart now" row — set while the saved proxy differs from the
    // one this process started with.
    void set_restart_pending(bool pending);

    // Fires when the user changes the mode, or edits the URL in Manual mode.
    // An invalid Manual URL never fires (an inline error is shown instead).
    std::function<void(tesseract::Settings::ProxyMode, std::string)> on_proxy_changed;

    // Fires when the user presses "Restart now".
    std::function<void()> on_restart_requested;

private:
    void on_mode_changed_(const std::string& value);
    void on_url_edited_();
    void refresh_url_state_();
    tesseract::Settings::ProxyMode mode_() const;

    tk::ComboBox* combo_ = nullptr;
    tk::TextField* url_field_ = nullptr;
    tk::Label* error_label_ = nullptr;
    tk::Widget* restart_row_ = nullptr;
};

} // namespace tesseract::views
